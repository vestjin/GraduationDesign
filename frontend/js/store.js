import { reactive } from 'vue';

export const store = reactive({
    token: localStorage.getItem('token') || '',
    isLoggedIn: !!localStorage.getItem('token'),
    currentView: 'files',   // files, recycle, shares, tags
    currentParentId: 0,
    pathStack: [{ id: 0, name: '根目录' }],
    fileList: [],
    recycleList: [],        // 预留
    shareList: [],          // 预留
    tagList: [],            // 预留
    selectedFileIds: [],
    showDetailDrawer: false,
    detailFileId: null,
    showShareDialog: false,
    shareFileId: null,
    theme: localStorage.getItem('theme') || 'light',
    uploadTasks: new Map(), // 上传任务 Map
    // 全局加载状态
    loading: false,
});