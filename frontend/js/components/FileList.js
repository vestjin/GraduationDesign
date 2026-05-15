window.FileListComponent = {
    name: 'FileList',
    template: `
        <div class="file-list-container" @dragover.prevent="onDragOver" @dragleave="onDragLeave" @drop.prevent="onDrop">
            <div style="margin-bottom:16px; display:flex; gap:12px">
                <el-input v-model="store.newFolderName" placeholder="新建文件夹" style="width:200px" clearable>
                    <template #append><el-button @click="createFolder" :loading="store.mkdirLoading">新建</el-button></template>
                </el-input>
                <el-upload action="#" :auto-upload="false" :show-file-list="false" :multiple="true" :on-change="handleFileSelect">
                    <el-button type="primary">上传文件</el-button>
                </el-upload>
                <el-button v-if="store.selectedFileIds.length" type="danger" @click="batchDelete">批量删除 ({{ store.selectedFileIds.length }})</el-button>
                <el-button v-if="store.selectedFileIds.length" @click="clearSelection">取消选择</el-button>
            </div>

            <el-table v-if="store.viewMode === 'list'" :data="store.fileList" stripe border @selection-change="handleSelectionChange" ref="tableRef" @row-contextmenu="onRowContextMenu">
                <el-table-column type="selection" width="55" />
                <el-table-column label="名称" min-width="200">
                    <template #default="{ row }">
                        <div @click="row.file_type===1 && enterFolder(row.file_id, row.file_name)" style="cursor:pointer">
                            <span class="file-icon">{{ window.getFileIcon(row.file_name) }}</span> {{ row.file_name }}
                        </div>
                    </template>
                </el-table-column>
                <el-table-column label="大小" width="120">
                    <template #default="{ row }">{{ row.file_type===1 ? '—' : window.formatSize(row.file_size) }}</template>
                </el-table-column>
                <el-table-column label="类型" width="100">
                    <template #default="{ row }">{{ row.file_type===1 ? '文件夹' : '文件' }}</template>
                </el-table-column>
                <el-table-column label="操作" width="300" fixed="right">
                    <template #default="{ row }">
                        <el-button v-if="row.file_type===0" size="small" type="info" @click.stop="previewFile(row)">预览</el-button>
                        <el-button v-if="row.file_type===0" size="small" type="success" @click.stop="downloadFile(row)">下载</el-button>
                        <el-button size="small" type="warning" @click.stop="renameFile(row)">改名</el-button>
                        <el-button size="small" type="primary" @click.stop="moveFile(row)">移动</el-button>
                        <el-button size="small" type="danger" @click.stop="deleteFile(row)">删除</el-button>
                    </template>
                </el-table-column>
            </el-table>

            <div v-else style="display:grid; grid-template-columns:repeat(auto-fill,minmax(130px,1fr)); gap:16px;">
                <el-card v-for="row in store.fileList" :key="row.file_id" shadow="hover" @click="row.file_type===1 && enterFolder(row.file_id, row.file_name)">
                    <div style="text-align:center">
                        <div style="font-size:42px">{{ window.getFileIcon(row.file_name) }}</div>
                        <div style="word-break:break-all">{{ row.file_name }}</div>
                        <div style="font-size:12px">{{ window.formatSize(row.file_size) }}</div>
                    </div>
                </el-card>
            </div>

            <div v-if="contextMenuVisible" class="context-menu" :style="{ top: contextMenuY+'px', left: contextMenuX+'px' }" @click.stop>
                <ul>
                    <li v-if="contextMenuFile.file_type===0" @click="previewFile(contextMenuFile)">预览</li>
                    <li v-if="contextMenuFile.file_type===0" @click="downloadFile(contextMenuFile)">下载</li>
                    <li @click="renameFile(contextMenuFile)">重命名</li>
                    <li @click="moveFile(contextMenuFile)">移动</li>
                    <li v-if="contextMenuFile.file_type===0" @click="openShare(contextMenuFile)">分享</li>
                    <li @click="deleteFile(contextMenuFile)">删除</li>
                    <li @click="showDetail(contextMenuFile)">详情</li>
                </ul>
            </div>
        </div>
    `,
    setup() {
        const store = Vue.inject('store');
        const contextMenuVisible = Vue.ref(false);
        const contextMenuX = Vue.ref(0);
        const contextMenuY = Vue.ref(0);
        const contextMenuFile = Vue.ref(null);
        const tableRef = Vue.ref(null);

        const loadFiles = async () => {
            const res = await window.api.getFileList(store.currentParentId);
            if (res.code === 0) store.fileList = res.data;
            else window.showMessage(res.msg || '加载失败', 'error');
        };

        const enterFolder = async (id, name) => {
            store.pathStack.push({ id, name });
            store.currentParentId = id;
            await loadFiles();
        };

        const createFolder = async () => {
            if (!store.newFolderName.trim()) return window.showMessage('请输入文件夹名', 'warning');
            store.mkdirLoading = true;
            const res = await window.api.mkdir(store.currentParentId, store.newFolderName);
            if (res.code === 0) { window.showMessage('创建成功'); store.newFolderName = ''; await loadFiles(); }
            else window.showMessage(res.msg, 'error');
            store.mkdirLoading = false;
        };

        const renameFile = async (file) => {
            const newName = prompt('新名称', file.file_name);
            if (!newName || newName === file.file_name) return;
            const res = await window.api.rename(file.file_id, newName);
            if (res.code === 0) { window.showMessage('重命名成功'); await loadFiles(); }
            else window.showMessage(res.msg, 'error');
        };

        const deleteFile = async (file) => {
            await ElementPlus.ElMessageBox.confirm(`确定删除 ${file.file_name} 吗？`, '提示', { type: 'warning' });
            const res = await window.api.deleteFile(file.file_id);
            if (res.code === 0) { window.showMessage('删除成功'); await loadFiles(); }
            else window.showMessage(res.msg, 'error');
        };

        const moveFile = async (file) => {
            const target = prompt('目标文件夹ID (0为根目录)', '0');
            if (target === null) return;
            const res = await window.api.move(file.file_id, parseInt(target));
            if (res.code === 0) { window.showMessage('移动成功'); await loadFiles(); }
            else window.showMessage(res.msg, 'error');
        };

        const downloadFile = async (file) => {
            const res = await window.api.download(file.file_id);
            const url = URL.createObjectURL(res.data);
            const a = document.createElement('a');
            a.href = url;
            a.download = file.file_name;
            a.click();
            URL.revokeObjectURL(url);
        };

        const previewFile = (file) => { window.open(window.api.getPreviewUrl(file.file_id), '_blank'); };

        const openShare = (file) => { store.shareFileId = file.file_id; store.showShareDialog = true; };
        const showDetail = (file) => { store.detailFileId = file.file_id; store.showDetailDrawer = true; };

        const batchDelete = async () => {
            for (const id of store.selectedFileIds) await window.api.deleteFile(id);
            window.showMessage('批量删除完成');
            store.selectedFileIds = [];
            await loadFiles();
        };
        const clearSelection = () => { tableRef.value?.clearSelection(); store.selectedFileIds = []; };
        const handleSelectionChange = (sel) => { store.selectedFileIds = sel.map(s => s.file_id); };

        const onRowContextMenu = (row, e) => {
            e.preventDefault();
            contextMenuFile.value = row;
            contextMenuX.value = e.clientX;
            contextMenuY.value = e.clientY;
            contextMenuVisible.value = true;
        };
        const closeContextMenu = () => { contextMenuVisible.value = false; };

        let dragCounter = 0;
        const onDragOver = (e) => { e.preventDefault(); e.dataTransfer.dropEffect = 'copy'; e.currentTarget.classList.add('drag-over'); };
        const onDragLeave = (e) => { dragCounter--; if (dragCounter === 0) e.currentTarget.classList.remove('drag-over'); };
        const onDrop = (e) => {
            e.preventDefault();
            e.currentTarget.classList.remove('drag-over');
            const files = Array.from(e.dataTransfer.files);
            if (files.length) files.forEach(f => window.__uploadManager?.addTask(f));
        };

        // 上传
        const handleFileSelect = (file) => { window.__uploadManager?.addTask(file.raw); };

        // 监听全局点击关闭右键菜单
        Vue.onMounted(() => {
            document.addEventListener('click', closeContextMenu);
            loadFiles();
        });
        Vue.onUnmounted(() => document.removeEventListener('click', closeContextMenu));

        return {
            store, contextMenuVisible, contextMenuX, contextMenuY, contextMenuFile, tableRef,
            window, createFolder, enterFolder, renameFile, deleteFile, moveFile, downloadFile, previewFile,
            openShare, showDetail, batchDelete, clearSelection, handleSelectionChange, onRowContextMenu,
            onDragOver, onDragLeave, onDrop, handleFileSelect
        };
    }
};