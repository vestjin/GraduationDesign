window.AppLayoutComponent = {
    name: 'AppLayout',
    components: {
        'app-sidebar': window.SidebarComponent,
        'app-topbar': window.TopBarComponent,
        'app-filelist': window.FileListComponent,
        'app-recyclebin': window.RecycleBinComponent,
        'app-uploadmanager': window.UploadManagerComponent,
        'app-filedetaildrawer': window.FileDetailDrawerComponent,
        'app-sharedialog': window.ShareDialogComponent
    },
    template: `
        <div class="app-layout">
            <!-- 未登录时显示登录/注册卡片 -->
            <div v-if="!store.isLoggedIn" style="display: flex; justify-content: center; align-items: center; height: 100vh; width: 100%;">
                <el-card style="max-width: 400px; width: 100%;">
                    <h2 style="text-align:center">私人云盘</h2>
                    <el-form :model="loginForm">
                        <el-form-item>
                            <el-input v-model="loginForm.username" placeholder="用户名" />
                        </el-form-item>
                        <el-form-item>
                            <el-input v-model="loginForm.password" type="password" placeholder="密码" show-password />
                        </el-form-item>
                        <el-form-item>
                            <el-button type="primary" @click="doLogin" :loading="loginLoading" style="width:100%">登录</el-button>
                            <el-button @click="doRegister" style="width:100%; margin-top:10px">注册</el-button>
                        </el-form-item>
                    </el-form>
                    <div v-if="loginMsg" style="color:red; text-align:center">{{ loginMsg }}</div>
                </el-card>
            </div>

            <!-- 已登录时显示主界面 -->
            <template v-else>
                <app-sidebar />
                <div class="main-content">
                    <app-topbar />
                    <div v-if="store.currentView === 'files'">
                        <app-filelist />
                    </div>
                    <div v-else-if="store.currentView === 'recycle'">
                        <app-recyclebin />
                    </div>
                    <div v-else-if="store.currentView === 'shares' || store.currentView === 'tags'">
                        <el-empty description="功能开发中，敬请期待" />
                    </div>
                    <app-uploadmanager />
                </div>
                <app-filedetaildrawer />
                <app-sharedialog />
            </template>
        </div>
    `,
    setup() {
        const store = Vue.inject('store');
        const loginForm = Vue.reactive({ username: '', password: '' });
        const loginLoading = Vue.ref(false);
        const loginMsg = Vue.ref('');

        const doLogin = async () => {
            loginLoading.value = true;
            loginMsg.value = '';
            try {
                const res = await window.api.login(loginForm.username, loginForm.password);
                if (res.code === 0) {
                    localStorage.setItem('token', res.data.token);
                    store.token = res.data.token;
                    store.isLoggedIn = true;
                    window.showMessage('登录成功');
                    // 登录后加载文件列表
                    const fileRes = await window.api.getFileList(store.currentParentId);
                    if (fileRes.code === 0) store.fileList = fileRes.data;
                } else {
                    loginMsg.value = res.msg;
                }
            } catch (err) {
                loginMsg.value = '网络错误，请检查后端服务';
            }
            loginLoading.value = false;
        };

        const doRegister = async () => {
            try {
                const res = await window.api.register(loginForm.username, loginForm.password);
                window.showMessage(res.msg, res.code === 0 ? 'success' : 'error');
                if (res.code === 0) {
                    loginMsg.value = '注册成功，请登录';
                    // 可选：自动填写登录表单
                    // loginForm.username = ...;
                }
            } catch (err) {
                window.showMessage('注册失败', 'error');
            }
        };

        return { store, loginForm, loginLoading, loginMsg, doLogin, doRegister };
    }
};