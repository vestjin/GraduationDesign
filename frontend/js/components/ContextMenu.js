import { api } from '../api.js';
import { showMessage } from '../utils.js';
import { store } from '../store.js';

export const ContextMenu = {
    props: ['x', 'y', 'file'],
    template: `
        <div class="context-menu" :style="{ top: y + 'px', left: x + 'px' }" @click.stop>
            <ul>
                <li @click="action('preview')" v-if="file.file_type===0">预览</li>
                <li @click="action('download')" v-if="file.file_type===0">下载</li>
                <li @click="action('rename')">重命名</li>
                <li @click="action('move')">移动</li>
                <li @click="action('share')" v-if="file.file_type===0">分享</li>
                <li @click="action('delete')">删除</li>
            </ul>
        </div>
    `,
    setup(props, { emit }) {
        const action = async (type) => {
            emit('close');
            if (type === 'preview') {
                window.open(api.getPreviewUrl(props.file.file_id), '_blank');
            } else if (type === 'download') {
                const res = await api.download(props.file.file_id);
                const url = window.URL.createObjectURL(new Blob([res.data]));
                const link = document.createElement('a');
                link.href = url;
                link.download = props.file.file_name;
                link.click();
                URL.revokeObjectURL(url);
            } else if (type === 'rename') {
                const newName = prompt('新名称', props.file.file_name);
                if (newName && newName !== props.file.file_name) {
                    const res = await api.rename(props.file.file_id, newName);
                    if (res.code === 0) {
                        showMessage('重命名成功');
                        // 刷新文件列表（通过事件，简化：直接触发 store 刷新？需全局方法，这里简单重新加载）
                        location.reload(); // 生产环境应使用 eventBus
                    } else showMessage(res.msg, 'error');
                }
            } else if (type === 'move') {
                const targetId = prompt('目标文件夹ID (0根目录)', '0');
                if (targetId !== null) {
                    const res = await api.move(props.file.file_id, parseInt(targetId));
                    showMessage(res.msg);
                    if (res.code === 0) location.reload();
                }
            } else if (type === 'share') {
                store.showShareDialog = true;
                store.shareFileId = props.file.file_id;
            } else if (type === 'delete') {
                if (confirm(`确定删除 ${props.file.file_name} 吗？`)) {
                    const res = await api.delete(props.file.file_id);
                    showMessage(res.msg);
                    if (res.code === 0) location.reload();
                }
            }
        };
        return { action };
    }
};