window.SidebarComponent = {
    name: 'Sidebar',
    template: `
        <div class="sidebar">
            <div style="padding: 20px; font-size: 20px; font-weight: bold; border-bottom: 1px solid var(--border-color);">
                ☁️ 私人云盘
            </div>
            <el-menu :default-active="store.currentView" @select="handleMenuSelect" background-color="var(--sidebar-bg)" text-color="var(--text-primary)">
                <el-menu-item index="files">
                    <el-icon><Folder /></el-icon><span>全部文件</span>
                </el-menu-item>
                <el-menu-item index="recycle">
                    <el-icon><Delete /></el-icon><span>回收站</span>
                </el-menu-item>
                <el-menu-item index="shares">
                    <el-icon><Share /></el-icon><span>我的分享</span>
                </el-menu-item>
                <el-menu-item index="tags">
                    <el-icon><PriceTag /></el-icon><span>标签管理</span>
                </el-menu-item>
            </el-menu>
        </div>
    `,
    setup() {
        const store = Vue.inject('store');
        const handleMenuSelect = async (index) => {
            store.currentView = index;
            if (index === 'files') {
                // 刷新文件列表（由 FileList 组件负责）
            } else if (index === 'recycle') {
                window.showMessage('回收站功能开发中', 'info');
            }
        };
        return { store, handleMenuSelect };
    }
};