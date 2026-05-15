window.TopBarComponent = {
    name: 'TopBar',
    template: `
        <div class="top-bar">
            <div style="display: flex; gap: 12px; flex:1; align-items: center; flex-wrap: wrap;">
                <el-breadcrumb separator="/">
                    <el-breadcrumb-item v-for="(item, idx) in store.pathStack" :key="idx">
                        <a href="#" @click.prevent="navigateTo(item.id, idx)">{{ item.name }}</a>
                    </el-breadcrumb-item>
                </el-breadcrumb>
                <el-input v-model="store.searchKeyword" placeholder="搜索文件..." prefix-icon="Search" clearable style="width: 200px;" @keyup.enter="doSearch" />
            </div>
            <div style="display: flex; gap: 12px; align-items: center;">
                <el-button-group>
                    <el-button :type="store.viewMode === 'list' ? 'primary' : 'default'" @click="store.viewMode='list'" icon="List">列表</el-button>
                    <el-button :type="store.viewMode === 'grid' ? 'primary' : 'default'" @click="store.viewMode='grid'" icon="Grid">网格</el-button>
                </el-button-group>
                <el-button @click="store.theme = store.theme === 'dark' ? 'light' : 'dark'" :icon="store.theme === 'dark' ? 'Sunny' : 'Moon'">主题</el-button>
                <el-button @click="logout" type="danger" plain size="small">退出</el-button>
            </div>
        </div>
    `,
    setup() {
        const store = Vue.inject('store');
        const navigateTo = async (id, idx) => {
            store.pathStack = store.pathStack.slice(0, idx + 1);
            store.currentParentId = id;
            const res = await window.api.getFileList(store.currentParentId);
            if (res.code === 0) store.fileList = res.data;
            else window.showMessage(res.msg || '加载失败', 'error');
        };
        const doSearch = () => {
            window.showMessage(`搜索功能开发中，关键词：${store.searchKeyword}`, 'info');
        };
        const logout = () => {
            localStorage.removeItem('token');
            store.token = '';
            store.isLoggedIn = false;
            location.reload();
        };
        return { store, navigateTo, doSearch, logout };
    }
};