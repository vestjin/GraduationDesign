const API_BASE = 'http://192.168.2.131:8080';  // 请修改为你的后端地址

const apiClient = (method, url, data, options = {}) => {
    const token = localStorage.getItem('token');
    const headers = { 'Content-Type': 'application/json', ...options.headers };
    if (token) headers['Token'] = token;
    return axios({ method, url: API_BASE + url, data, headers, ...options });
};

window.api = {
    async login(username, password) {
        const res = await apiClient('post', '/api/user/login', { username, password });
        return res.data;
    },
    async register(username, password) {
        const res = await apiClient('post', '/api/user/register', { username, password });
        return res.data;
    },
    async getFileList(parentId, fetchAll = false) {
        const res = await apiClient('post', '/api/files/list', { parent_id: parentId, fetch_all: fetchAll ? 1 : 0 });
        return res.data;
    },
    async mkdir(parentId, folderName) {
        const res = await apiClient('post', '/api/files/mkdir', { parent_id: parentId, folder_name: folderName });
        return res.data;
    },
    async rename(fileId, newName) {
        const res = await apiClient('post', '/api/files/rename', { file_id: fileId, new_name: newName });
        return res.data;
    },
    async deleteFile(fileId) {
        const res = await apiClient('post', '/api/files/delete', { file_id: fileId });
        return res.data;
    },
    async move(fileId, targetParentId) {
        const res = await apiClient('post', '/api/files/move', { file_id: fileId, target_parent_id: targetParentId });
        return res.data;
    },
    async uploadCheck(md5, fileSize, fileName, parentId) {
        const res = await apiClient('post', '/api/files/upload/check', { md5, file_size: fileSize, file_name: fileName, parent_id: parentId });
        return res.data;
    },
    async uploadChunk(md5, offset, chunk, signal) {
        const token = localStorage.getItem('token');
        const url = `${API_BASE}/api/files/upload/chunk?md5=${md5}&offset=${offset}`;
        const res = await axios.post(url, chunk, { headers: { 'Token': token, 'Content-Type': 'application/octet-stream' }, signal });
        return res.data;
    },
    async uploadComplete(md5, fileName, parentId) {
        const res = await apiClient('post', '/api/files/upload/complete', { md5, file_name: fileName, parent_id: parentId });
        return res.data;
    },
    async download(fileId) {
        const token = localStorage.getItem('token');
        const res = await axios.post(`${API_BASE}/api/files/download`, { file_id: fileId }, { headers: { 'Token': token }, responseType: 'blob' });
        return res;
    },
    getPreviewUrl(fileId) {
        const token = localStorage.getItem('token');
        return `${API_BASE}/api/files/view?file_id=${fileId}&token=${token}`;
    }
};