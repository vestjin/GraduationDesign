window.FileDetailDrawerComponent = {
    name: 'FileDetailDrawer',
    template: `
        <el-drawer v-model="store.showDetailDrawer" title="文件详情" size="30%">
            <div v-if="currentFile">
                <el-descriptions :column="1" border>
                    <el-descriptions-item label="文件名">{{ currentFile.file_name }}</el-descriptions-item>
                    <el-descriptions-item label="大小">{{ window.formatSize(currentFile.file_size) }}</el-descriptions-item>
                    <el-descriptions-item label="类型">{{ currentFile.file_type===1 ? '文件夹' : '文件' }}</el-descriptions-item>
                </el-descriptions>
                <el-divider />
                <el-button type="primary" plain @click="share">分享</el-button>
            </div>
        </el-drawer>
    `,
    setup() {
        const store = Vue.inject('store');
        const currentFile = Vue.computed(() => store.fileList.find(f => f.file_id === store.detailFileId));
        const share = () => { store.shareFileId = store.detailFileId; store.showShareDialog = true; };
        return { store, currentFile, share, window };
    }
};