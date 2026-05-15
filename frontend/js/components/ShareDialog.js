window.ShareDialogComponent = {
    name: 'ShareDialog',
    template: `
        <el-dialog v-model="store.showShareDialog" title="分享文件" width="30%">
            <el-empty description="分享功能开发中" />
            <template #footer><el-button @click="store.showShareDialog=false">关闭</el-button></template>
        </el-dialog>
    `,
    setup() {
        const store = Vue.inject('store');
        return { store };
    }
};