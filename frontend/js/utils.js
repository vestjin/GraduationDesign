window.formatSize = function(bytes) {
    if (bytes === 0) return '0 B';
    const k = 1024;
    const sizes = ['B', 'KB', 'MB', 'GB', 'TB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
};

window.getFileIcon = function(filename) {
    const ext = filename.split('.').pop().toLowerCase();
    const icons = { jpg:'🖼️', jpeg:'🖼️', png:'🖼️', gif:'🖼️', bmp:'🖼️', mp4:'🎬', mkv:'🎬', avi:'🎬', mov:'🎬', mp3:'🎵', wav:'🎵', flac:'🎵', pdf:'📄', doc:'📃', docx:'📃', xls:'📊', xlsx:'📊', zip:'🗜️', rar:'🗜️', '7z':'🗜️' };
    return icons[ext] || '📄';
};

window.showMessage = function(msg, type = 'success') {
    ElementPlus.ElMessage({ message: msg, type });
};

window.toggleTheme = function(theme) {
    if (theme === 'dark') document.body.classList.add('dark');
    else document.body.classList.remove('dark');
    localStorage.setItem('theme', theme);
};