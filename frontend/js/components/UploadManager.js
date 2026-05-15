const CHUNK_SIZE = 5 * 1024 * 1024;
class UploadTask {
    constructor(file, parentId, onUpdate, onComplete, onError) {
        this.file = file;
        this.parentId = parentId;
        this.id = `${file.name}_${file.size}_${file.lastModified}`.replace(/\W/g, '_');
        this.status = 'pending';
        this.progress = 0;
        this.offset = 0;
        this.md5 = null;
        this.controller = null;
        this.speed = 0;
        this.remainingTime = '';
        this.onUpdate = onUpdate;
        this.onComplete = onComplete;
        this.onError = onError;
        this.loadFromStorage();
    }
    loadFromStorage() {
        const saved = localStorage.getItem(`upload_${this.id}`);
        if (saved) {
            const data = JSON.parse(saved);
            if (data.md5 && data.offset > 0) {
                this.md5 = data.md5;
                this.offset = data.offset;
                this.progress = Math.floor((this.offset / this.file.size) * 100);
            }
        }
    }
    saveToStorage() {
        if (this.md5 && this.offset > 0) {
            localStorage.setItem(`upload_${this.id}`, JSON.stringify({ md5: this.md5, offset: this.offset, timestamp: Date.now() }));
        }
    }
    clearStorage() { localStorage.removeItem(`upload_${this.id}`); }
    async start() {
        if (this.status !== 'pending' && this.status !== 'paused') return;
        this.status = 'uploading';
        this.controller = new AbortController();
        this.onUpdate(this);
        try {
            if (!this.md5) {
                this.statusText = '计算MD5...';
                this.onUpdate(this);
                this.md5 = await this.calcMD5();
            }
            const check = await window.api.uploadCheck(this.md5, this.file.size, this.file.name, this.parentId);
            if (check.code !== 0) throw new Error(check.msg);
            if (check.data.status === 'instant') { this.finish(); return; }
            const serverOffset = check.data.offset || 0;
            this.offset = Math.max(this.offset, serverOffset);
            this.saveToStorage();
            const total = this.file.size;
            let uploadedBytes = this.offset;
            const startTime = Date.now();
            while (this.offset < total && this.status === 'uploading') {
                const end = Math.min(this.offset + CHUNK_SIZE, total);
                const chunk = this.file.slice(this.offset, end);
                const chunkRes = await window.api.uploadChunk(this.md5, this.offset, chunk, this.controller.signal);
                if (chunkRes.code !== 0) throw new Error(chunkRes.msg);
                const newOffset = chunkRes.data.offset;
                if (newOffset > this.offset) this.offset = newOffset;
                else this.offset = end;
                this.progress = Math.floor((this.offset / total) * 100);
                const now = Date.now();
                const elapsed = (now - startTime) / 1000;
                const uploaded = this.offset;
                const delta = uploaded - uploadedBytes;
                if (elapsed > 0) { this.speed = Math.round(delta / elapsed); const remain = total - this.offset; this.remainingTime = remain / this.speed > 0 ? Math.ceil(remain / this.speed) + '秒' : ''; }
                uploadedBytes = uploaded;
                this.saveToStorage();
                this.onUpdate(this);
            }
            if (this.status === 'uploading') {
                const complete = await window.api.uploadComplete(this.md5, this.file.name, this.parentId);
                if (complete.code === 0) this.finish();
                else throw new Error(complete.msg);
            }
        } catch (err) {
            if (err.name === 'AbortError') {
                this.status = 'paused';
                this.saveToStorage();
            } else {
                this.status = 'error';
                this.errorMsg = err.message;
                this.onError(this);
            }
            this.onUpdate(this);
        }
    }
    calcMD5() {
        return new Promise((resolve) => {
            const spark = new SparkMD5.ArrayBuffer();
            const reader = new FileReader();
            let offset = 0;
            const chunkSize = 5 * 1024 * 1024;
            const loadNext = () => {
                const end = Math.min(offset + chunkSize, this.file.size);
                reader.readAsArrayBuffer(this.file.slice(offset, end));
            };
            reader.onload = e => {
                spark.append(e.target.result);
                offset += chunkSize;
                if (offset < this.file.size) setTimeout(loadNext, 0);
                else resolve(spark.end());
            };
            loadNext();
        });
    }
    pause() { if (this.status === 'uploading' && this.controller) this.controller.abort(); }
    resume() { if (this.status === 'paused') { this.status = 'pending'; this.onUpdate(this); this.start(); } }
    finish() { this.status = 'done'; this.progress = 100; this.clearStorage(); this.onUpdate(this); this.onComplete(this); }
}

window.UploadManagerComponent = {
    name: 'UploadManager',
    template: `
        <div class="upload-tasks" v-if="tasks.length">
            <div style="font-weight:500; margin-bottom:12px">上传任务 ({{ tasks.length }})</div>
            <div v-for="task in tasks" :key="task.id" style="margin-bottom:12px">
                <el-card>
                    <div style="display:flex; justify-content:space-between; align-items:center">
                        <div style="flex:1">
                            <div><strong>{{ task.file.name }}</strong> ({{ window.formatSize(task.file.size) }})</div>
                            <el-progress :percentage="task.progress" :status="task.status==='error'?'exception':(task.status==='done'?'success':'')" />
                            <div style="font-size:12px">
                                <span v-if="task.status==='pending'">等待中</span>
                                <span v-if="task.status==='uploading'">上传中 速度:{{ task.speed }}KB/s 剩余:{{ task.remainingTime }}</span>
                                <span v-if="task.status==='paused'">已暂停</span>
                                <span v-if="task.status==='error'">错误: {{ task.errorMsg }}</span>
                                <span v-if="task.status==='done'">完成</span>
                            </div>
                        </div>
                        <div>
                            <el-button v-if="task.status==='uploading'" size="small" @click="task.pause()">暂停</el-button>
                            <el-button v-if="task.status==='paused'" size="small" type="primary" @click="task.resume()">继续</el-button>
                        </div>
                    </div>
                </el-card>
            </div>
        </div>
    `,
    setup() {
        const store = Vue.inject('store');
        const tasks = Vue.ref(store.uploadTasks);
        const addTask = (file) => {
            const existing = tasks.value.find(t => t.id === `${file.name}_${file.size}_${file.lastModified}`.replace(/\W/g, '_'));
            if (existing && existing.status !== 'done') return;
            const task = new UploadTask(file, store.currentParentId,
                (t) => { tasks.value = [...tasks.value]; store.uploadTasks = tasks.value; },
                (t) => { tasks.value = tasks.value.filter(tt => tt.id !== t.id); store.uploadTasks = tasks.value; window.showMessage(`${t.file.name} 上传完成`); },
                (t) => { window.showMessage(`${t.file.name} 上传失败: ${t.errorMsg || '未知错误'}`, 'error'); }
            );
            tasks.value.push(task);
            store.uploadTasks = tasks.value;
            setTimeout(() => task.start(), 0);
        };
        window.__uploadManager = { addTask };
        return { tasks, window };
    }
};