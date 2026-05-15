// 全局 store（简化版状态管理）
const store = Vue.reactive({
    token: localStorage.getItem('token') || '',
    isLoggedIn: !!localStorage.getItem('token'),
    currentView: 'files',
    currentParentId: 0,
    pathStack: [{ id: 0, name: '根目录' }],
    fileList: [],
    selectedFileIds: [],
    showDetailDrawer: false,
    detailFileId: null,
    showShareDialog: false,
    shareFileId: null,
    uploadTasks: [],
    theme: localStorage.getItem('theme') || 'light',
    searchKeyword: '',
    viewMode: 'list',
    mkdirLoading: false,
    newFolderName: ''
});

// 应用启动
const app = Vue.createApp({
    components: {
        'AppLayout': window.AppLayoutComponent
    },
    template: `<AppLayout />`,
    provide() {
        return { store };
    }
});

app.use(ElementPlus);
app.mount('#app');

// 初始化主题
window.toggleTheme(store.theme);
watch(store, () => { window.toggleTheme(store.theme); }, { deep: true }); // 简单监听