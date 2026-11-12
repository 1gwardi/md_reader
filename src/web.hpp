#pragma once

#include <string_view>

inline constexpr std::string_view toolbar_start = R"HTML(
<header class="app-toolbar">
  <div class="toolbar-navigation" aria-label="Навигация">
    <button id="toggle-files" class="icon-button directory-control" title="Скрыть список файлов" aria-label="Скрыть список файлов" aria-expanded="true" aria-controls="explorer"><svg viewBox="0 0 24 24" aria-hidden="true"><rect x="3" y="4" width="18" height="16" rx="3"/><path d="M9 4v16"/></svg></button>
    <div class="navigation-pair directory-control">
      <button id="go-back" class="icon-button" title="Назад" aria-label="Назад" disabled><svg viewBox="0 0 24 24" aria-hidden="true"><path d="m14 6-6 6 6 6"/></svg></button>
      <button id="go-forward" class="icon-button" title="Вперёд" aria-label="Вперёд" disabled><svg viewBox="0 0 24 24" aria-hidden="true"><path d="m10 6 6 6-6 6"/></svg></button>
    </div>
    <a id="go-up" class="icon-button directory-control" title="На уровень выше" aria-label="На уровень выше"><svg viewBox="0 0 24 24" aria-hidden="true"><path d="M12 19V5m-6 6 6-6 6 6"/></svg></a>
    <a id="go-home" class="icon-button directory-control" href="/" title="В корень открытой папки" aria-label="В корень открытой папки"><svg viewBox="0 0 24 24" aria-hidden="true"><path d="m3 11 9-8 9 8M5 9v11h5v-6h4v6h5V9"/></svg></a>
  </div>
)HTML";

inline constexpr std::string_view toolbar_end = R"HTML(
  <div class="toolbar-actions">
    <span id="status" role="status" aria-live="polite">Загрузка…</span>
    <button id="refresh" class="action-button" title="Обновить документ и список файлов"><svg viewBox="0 0 24 24" aria-hidden="true"><path d="M20 7v5h-5M4 17v-5h5M6.1 7a7 7 0 0 1 11.5-1L20 9M4 15l2.4 3A7 7 0 0 0 18 17"/></svg><span>Обновить</span></button>
    <button id="toggle-theme" class="icon-button" title="Тема: системная" aria-label="Тема: системная"><svg viewBox="0 0 24 24" aria-hidden="true"><circle cx="12" cy="12" r="8"/><path d="M12 4a8 8 0 0 1 0 16Z" fill="currentColor" stroke="none"/></svg></button>
  </div>
</header>
)HTML";

inline constexpr std::string_view stylesheet = R"CSS(
:root { color-scheme:light; --bg:#f8f9fa; --surface:#fff; --sidebar:#f3f5f5; --fg:#26312e; --muted:#78827e; --line:#e3e8e6; --code:#f1f4f3; --link:#34765e; --accent:#34765e; --selected:#e4eee8; --hover:#eaf0ed; --error:#b54c42; --sidebar-width:250px; }
@media(prefers-color-scheme:dark) { :root:not([data-theme=light]) { color-scheme:dark; --bg:#191d1c; --surface:#1e2321; --sidebar:#161b19; --fg:#e0e7e3; --muted:#96a39c; --line:#303a34; --code:#272f2a; --link:#9bc6ad; --accent:#9bc6ad; --selected:#2b4033; --hover:#26352c; --error:#ef9c91; } }
:root[data-theme=dark] { color-scheme:dark; --bg:#191d1c; --surface:#1e2321; --sidebar:#161b19; --fg:#e0e7e3; --muted:#96a39c; --line:#303a34; --code:#272f2a; --link:#9bc6ad; --accent:#9bc6ad; --selected:#2b4033; --hover:#26352c; --error:#ef9c91; }
* { box-sizing:border-box; }
body { margin:0; background:var(--bg); color:var(--fg); font:16px/1.8 -apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif; overflow-wrap:break-word; }
[hidden] { display:none !important; }
button,input { font:inherit; } button,a,input { -webkit-tap-highlight-color:transparent; }
button { cursor:pointer; } button:disabled { cursor:default; opacity:.3; }
:focus-visible { outline:2px solid var(--accent); outline-offset:3px; }
svg { width:19px; height:19px; fill:none; stroke:currentColor; stroke-width:1.6; stroke-linecap:round; stroke-linejoin:round; flex-shrink:0; }
.with-explorer { padding-left:var(--sidebar-width); }
.with-explorer.sidebar-hidden { padding-left:0; }
.app-toolbar { position:sticky; top:0; z-index:20; min-height:76px; padding:14px 24px; display:flex; align-items:center; gap:18px; background:var(--surface); border-bottom:1px solid var(--line); }
.toolbar-navigation,.toolbar-actions,.navigation-pair { display:flex; align-items:center; gap:4px; flex-shrink:0; }
.navigation-pair { gap:0; }
.icon-button,.action-button { display:inline-flex; align-items:center; justify-content:center; gap:8px; height:34px; border:1px solid transparent; background:transparent; color:var(--muted); border-radius:7px; text-decoration:none; line-height:1; }
.icon-button { width:32px; padding:0; }
.icon-button:hover:not(:disabled):not([aria-disabled=true]),.action-button:hover { background:var(--hover); color:var(--fg); }
.icon-button[aria-disabled=true] { opacity:.3; pointer-events:none; }
.action-button { padding:0 11px; border-color:var(--line); background:var(--surface); color:var(--fg); font-size:12px; font-weight:550; }
.action-button svg { width:16px; height:16px; }
.document-heading { display:flex; flex-direction:column; gap:1px; min-width:0; flex:1; line-height:1.45; }
.eyebrow { font-size:9px; font-weight:650; letter-spacing:.12em; color:var(--muted); }
#filename { font-size:14px; font-weight:600; overflow:hidden; text-overflow:ellipsis; white-space:nowrap; }
#status { display:flex; align-items:center; gap:7px; padding:0 10px; color:var(--muted); font-size:10px; white-space:nowrap; }
#status::before { content:''; width:5px; height:5px; border-radius:50%; background:var(--accent); flex-shrink:0; }
#status[data-error] { color:var(--error); } #status[data-error]::before { background:var(--error); }
#notice { max-width:800px; margin:20px auto 0; padding:10px 16px; border:1px solid var(--line); border-radius:8px; background:var(--surface); color:var(--error); font-size:13px; }
body:not(.with-explorer) .directory-control { display:none; }
body:not(.with-explorer) .toolbar-navigation { display:none; }
#explorer { position:fixed; inset:0 auto 0 0; width:var(--sidebar-width); padding:0 16px 24px; overflow-y:auto; background:var(--sidebar); border-right:1px solid var(--line); font-size:13px; }
.sidebar-hidden #explorer { visibility:hidden; }
.root-link { display:flex; align-items:center; gap:10px; height:76px; padding:0 8px; margin-bottom:25px; color:var(--fg); font-size:17px; font-weight:650; letter-spacing:-.04em; text-decoration:none; }
.brand-mark { display:flex; align-items:center; justify-content:center; width:29px; height:29px; border-radius:8px; background:var(--fg); color:var(--surface); font-size:16px; font-weight:650; letter-spacing:-.07em; }
.explorer-heading { display:flex; flex-direction:column; gap:4px; padding:0 10px; }
.folder-name { font-size:13px; font-weight:600; overflow-wrap:anywhere; }
#breadcrumbs { display:flex; flex-wrap:wrap; gap:3px 6px; padding:10px 10px 18px; font-size:10px; color:var(--muted); line-height:1.5; }
#breadcrumbs a { color:var(--muted); text-decoration:none; } #breadcrumbs a:hover { color:var(--accent); text-decoration:underline; }
.search-field { display:flex; align-items:center; gap:8px; height:34px; margin:0 2px 20px; padding:0 10px; border:1px solid var(--line); border-radius:7px; background:var(--surface); color:var(--muted); }
.search-field:focus-within { border-color:var(--accent); }
.search-field > svg { width:15px; height:15px; }
#file-search { min-width:0; width:100%; padding:0; border:0; background:transparent; color:var(--fg); outline:none; font-size:11px; }
#file-search::placeholder { color:var(--muted); }
.list-caption { margin:0 10px 9px; font-size:9px; color:var(--muted); letter-spacing:.03em; }
.file-list { list-style:none; margin:0; padding:0; }
.file-list a { display:flex; align-items:center; gap:10px; padding:9px 10px; margin-bottom:3px; border-radius:7px; text-decoration:none; color:var(--fg); line-height:1.5; }
.file-list a:hover { background:var(--hover); }
.file-list a[aria-current=page] { background:var(--selected); color:var(--accent); font-weight:550; }
.file-kind { width:17px; height:17px; color:var(--muted); } .file-list a[aria-current=page] .file-kind { color:var(--accent); }
.entry-name { flex:1; min-width:0; overflow-wrap:anywhere; }
.entry-description { color:var(--muted); font-size:11px; display:none; }
.explorer-hint { margin:28px 10px 0; padding-top:16px; border-top:1px solid var(--line); color:var(--muted); font-size:10px; line-height:1.7; }
.empty-list { color:var(--muted); padding:16px 10px; font-size:12px; }
#directory-list { margin-top:28px; border:1px solid var(--line); border-radius:12px; overflow:hidden; background:var(--surface); }
#directory-list .file-list a { padding:16px 20px; margin:0; border-radius:0; border-bottom:1px solid var(--line); font-size:14px; }
#directory-list .file-list li:last-child a { border:0; }
#directory-list .file-kind { width:20px; height:20px; } #directory-list .entry-description { display:block; }
main { max-width:800px; margin:28px auto 60px; padding:42px 48px 64px; background:var(--surface); border:1px solid var(--line); border-radius:12px; scroll-margin-top:96px; }
main:focus { outline:none; }
body[data-reading=false] main { background:transparent; border:0; padding-top:26px; }
main h1,main h2,main h3,main h4,main h5,main h6 { line-height:1.35; margin:1.6em 0 .7em; font-weight:650; letter-spacing:-.025em; scroll-margin-top:100px; }
main h1 { font-size:1.95em; } main h2 { font-size:1.4em; border-bottom:1px solid var(--line); padding-bottom:.45em; }
main > :first-child { margin-top:0; }
p,ul,ol,pre,blockquote,table { margin:0 0 1.2em; } li > p { margin-bottom:.4em; }
a { color:var(--link); text-underline-offset:3px; }
img { max-width:100%; height:auto; border-radius:6px; }
pre,code { font-family:ui-monospace,SFMono-Regular,Consolas,monospace; font-size:.85em; background:var(--code); border-radius:5px; }
code { padding:.15em .35em; } pre { overflow:auto; padding:18px 22px; line-height:1.65; } pre code { padding:0; font-size:1em; }
blockquote { margin-left:0; padding:0 20px; border-left:3px solid var(--line); color:var(--muted); }
table { display:block; overflow:auto; border-collapse:collapse; } th,td { padding:9px 16px; border:1px solid var(--line); } th { background:var(--code); }
hr { border:0; border-top:1px solid var(--line); margin:2em 0; }
input[type=checkbox] { margin-right:.5em; }
details { margin:0 0 1.2em; padding:12px 18px; border:1px solid var(--line); border-radius:6px; } summary { cursor:pointer; font-weight:600; } details[open] > summary { margin-bottom:12px; }
.muted { color:var(--muted); }
.skip-link { position:fixed; top:-100px; left:20px; z-index:100; padding:8px 14px; border-radius:6px; background:var(--surface); } .skip-link:focus { top:12px; }
@media(min-width:761px) { #sidebar-backdrop { display:none !important; } }
@media(max-width:1100px) { main { margin:24px; } .app-toolbar { gap:10px; padding:14px 18px; } }
@media(max-width:900px) { #status { display:none; } }
@media(max-width:760px) { .with-explorer { padding-left:0; } #explorer { top:76px; z-index:30; width:270px; box-shadow:8px 0 30px #0001; } .root-link { height:60px; margin-bottom:12px; } #sidebar-backdrop { display:block; position:fixed; inset:76px 0 0; z-index:25; border:0; background:#0004; } .app-toolbar { padding:12px; gap:8px; } .toolbar-navigation { gap:0; } .navigation-pair { display:none; } .action-button span { display:none; } .action-button { width:32px; padding:0; } .toolbar-actions { gap:0; } #filename { font-size:12px; } main { margin:16px 12px 32px; padding:28px 22px 40px; font-size:15px; } body[data-reading=false] main { padding:24px 10px; } #notice { margin:12px; } }
@media(prefers-reduced-motion:no-preference) { .icon-button,.action-button,.file-list a { transition:background .12s,color .12s; } button.is-refreshing svg { animation:spin 1s linear infinite; } @keyframes spin { to { transform:rotate(360deg); } } }
)CSS";

inline constexpr std::string_view browser_script = R"JS(
const article = document.querySelector('main');
const status = document.querySelector('#status');
const directoryMode = document.body.dataset.directoryMode === 'true';
const reading = document.body.dataset.reading === 'true';
const folder = document.body.dataset.folder;
const selected = document.body.dataset.selected;
const explorer = document.querySelector('#explorer');
const sidebar = document.querySelector('#files');
const directoryList = reading ? null : article.querySelector('#directory-list');
const search = document.querySelector('#file-search');
const fileCount = document.querySelector('#file-count');
const refreshButton = document.querySelector('#refresh');
const notice = document.querySelector('#notice');
const toggleFiles = document.querySelector('#toggle-files');
const backdrop = document.querySelector('#sidebar-backdrop');
const backButton = document.querySelector('#go-back');
const forwardButton = document.querySelector('#go-forward');
const themeButton = document.querySelector('#toggle-theme');
const contentURL = '/__mdread/content' + (directoryMode ? '?path=' + encodeURIComponent(selected) : '');
let revision = '';
let loaded = false;
let lastListing = '';
let entries = [];
let refreshTimer;
let refreshing = false;
function readSetting(key) { try { return sessionStorage.getItem('mdread-' + key); } catch { return null; } }
function saveSetting(key, value) { try { sessionStorage.setItem('mdread-' + key, value); } catch {} }
function setSidebar(hidden) {
  document.body.classList.toggle('sidebar-hidden', hidden);
  toggleFiles.setAttribute('aria-expanded', String(!hidden));
  toggleFiles.title = hidden ? 'Показать список файлов' : 'Скрыть список файлов';
  toggleFiles.setAttribute('aria-label', toggleFiles.title);
  backdrop.hidden = hidden || !matchMedia('(max-width:760px)').matches;
  saveSetting('sidebar-hidden', String(hidden));
}
function updateHistoryButtons() {
  const index = history.state?.mdreadIndex ?? 0;
  saveSetting('nav-index', String(index));
  backButton.disabled = index <= 0;
  forwardButton.disabled = index >= Number(readSetting('nav-max') ?? index);
}
function folderURL(path) { return path ? '/files/' + path.split('/').map(encodeURIComponent).join('/') + '/' : '/'; }
if (directoryMode) {
  if (typeof history.state?.mdreadIndex !== 'number') {
    const index = Number(readSetting('nav-index') ?? -1) + 1;
    history.replaceState({mdreadIndex:index}, '');
    saveSetting('nav-max', String(index));
  }
  updateHistoryButtons();
  window.addEventListener('pageshow', updateHistoryButtons);
  backButton.addEventListener('click', () => history.back());
  forwardButton.addEventListener('click', () => history.forward());
  const up = document.querySelector('#go-up');
  const home = document.querySelector('#go-home');
  if (reading || folder) {
    up.href = folderURL(reading ? folder : folder.split('/').slice(0, -1).join('/'));
    up.title = reading ? 'К папке документа' : 'На уровень выше';
    up.setAttribute('aria-label', up.title);
  } else {
    for (const link of [up, home]) { link.setAttribute('aria-disabled', 'true'); link.removeAttribute('href'); link.tabIndex = -1; }
  }
  setSidebar(matchMedia('(max-width:760px)').matches || readSetting('sidebar-hidden') === 'true');
  matchMedia('(max-width:760px)').addEventListener('change', event => setSidebar(event.matches));
  toggleFiles.addEventListener('click', () => setSidebar(!document.body.classList.contains('sidebar-hidden')));
  backdrop.addEventListener('click', () => setSidebar(true));
  search.value = readSetting('search:' + folder) ?? '';
  search.addEventListener('input', () => { saveSetting('search:' + folder, search.value); displayListing(); });
  document.addEventListener('keydown', event => {
    if ((event.metaKey || event.ctrlKey) && event.key.toLowerCase() === 'k') {
      event.preventDefault(); setSidebar(false); search.focus(); search.select();
    } else if (event.key === 'Escape' && matchMedia('(max-width:760px)').matches) setSidebar(true);
  });
}
let theme = readSetting('theme') ?? 'auto';
function applyTheme() {
  if (theme === 'auto') document.documentElement.removeAttribute('data-theme');
  else document.documentElement.dataset.theme = theme;
  themeButton.title = 'Тема: ' + ({auto:'системная',light:'светлая',dark:'тёмная'}[theme]);
  themeButton.setAttribute('aria-label', themeButton.title + '. Переключить тему');
}
applyTheme();
themeButton.addEventListener('click', () => {
  theme = {auto:'light',light:'dark',dark:'auto'}[theme] ?? 'auto';
  saveSetting('theme', theme); applyTheme();
});
refreshButton.addEventListener('click', () => refresh(true));
function fileIcon(directory) {
  const svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
  svg.setAttribute('viewBox', '0 0 24 24');
  svg.setAttribute('aria-hidden', 'true');
  svg.classList.add('file-kind');
  const path = document.createElementNS('http://www.w3.org/2000/svg', 'path');
  path.setAttribute('d', directory
    ? 'M3 7V5a1 1 0 0 1 1-1h6l2 3h8a1 1 0 0 1 1 1v11a1 1 0 0 1-1 1H4a1 1 0 0 1-1-1V7Z'
    : 'M14 3H6a1 1 0 0 0-1 1v16a1 1 0 0 0 1 1h12a1 1 0 0 0 1-1V8l-5-5Zm0 0v5h5M8 12h8m-8 4h5');
  svg.append(path);
  return svg;
}
function fileList(entries) {
  const list = document.createElement('ul');
  list.className = 'file-list';
  for (const entry of entries) {
    const item = document.createElement('li');
    const link = document.createElement('a');
    link.href = entry.url;
    if (entry.path === selected) link.setAttribute('aria-current', 'page');
    const name = document.createElement('span');
    name.className = 'entry-name';
    name.textContent = entry.name;
    const description = document.createElement('span');
    description.className = 'entry-description';
    description.textContent = entry.directory ? 'Папка' : 'Markdown';
    link.append(fileIcon(entry.directory), name, description);
    item.append(link);
    list.append(item);
  }
  if (!entries.length) {
    const item = document.createElement('li');
    item.className = 'empty-list';
    item.textContent = search.value.trim() ? 'Ничего не найдено. Измените запрос.' : 'В этой папке пока нет Markdown-файлов и папок.';
    list.append(item);
  }
  return list;
}
function displayListing() {
  const query = search.value.trim().toLocaleLowerCase();
  const filtered = entries.filter(entry => entry.name.toLocaleLowerCase().includes(query));
  const position = explorer.scrollTop;
  sidebar.replaceChildren(fileList(filtered));
  explorer.scrollTop = position;
  directoryList?.replaceChildren(fileList(filtered));
  const files = filtered.filter(entry => !entry.directory).length;
  fileCount.textContent = 'Файлы: ' + files + ' · Папки: ' + (filtered.length - files);
}
function showBreadcrumbs() {
  const nav = document.querySelector('#breadcrumbs');
  const root = document.createElement('a');
  root.href = '/';
  root.textContent = 'Корень';
  nav.append(root);
  const parts = folder.split('/').filter(Boolean);
  parts.forEach((part, index) => {
    const link = document.createElement('a');
    link.href = '/files/' + parts.slice(0, index + 1).map(encodeURIComponent).join('/') + '/';
    link.textContent = part;
    nav.append(document.createTextNode('/'), link);
  });
  if (parts.length) {
    const parent = document.createElement('a');
    parent.href = parts.length === 1 ? '/' : '/files/' + parts.slice(0, -1).map(encodeURIComponent).join('/') + '/';
    parent.textContent = '↑ На уровень выше';
    nav.append(parent);
  }
}
async function refreshListing(force) {
  const response = await fetch('/__mdread/list?path=' + encodeURIComponent(folder), {
    cache: 'no-store', signal: AbortSignal.timeout(4000)
  });
  const data = await response.json();
  if (!response.ok) throw new Error(data.error);
  const listing = JSON.stringify(data.entries);
  if (force || listing !== lastListing) {
    entries = data.entries;
    displayListing();
    lastListing = listing;
  }
}
if (directoryMode) showBreadcrumbs();
async function refresh(force = false) {
  if (refreshing) return;
  refreshing = true;
  clearTimeout(refreshTimer);
  refreshButton.disabled = true;
  refreshButton.classList.add('is-refreshing');
  refreshButton.setAttribute('aria-busy', 'true');
  try {
    if (directoryMode) await refreshListing(force);
    if (reading) {
      const response = await fetch(contentURL, {
        headers: revision && !force ? {'If-None-Match': revision} : {},
        cache: 'no-store', signal: AbortSignal.timeout(4000)
      });
      if (response.status !== 304) {
        const data = await response.json();
        if (!response.ok) throw new Error(data.error);
        const position = window.scrollY;
        const content = DOMPurify.sanitize(data.html, {
          USE_PROFILES: {html: true}, RETURN_DOM_FRAGMENT: true,
          FORBID_TAGS: ['style', 'link', 'meta', 'base']
        });
        article.replaceChildren(content);
        const usedIds = new Set(Array.from(article.querySelectorAll('[id]'), element => element.id));
        article.querySelectorAll('h1,h2,h3,h4,h5,h6').forEach(heading => {
          if (heading.id) return;
          const base = heading.textContent.toLowerCase().trim()
            .replace(/[^\p{L}\p{N}\p{M}_\s-]/gu, '').replace(/\s/g, '-') || 'section';
          let id = base;
          for (let suffix = 1; usedIds.has(id); suffix++) id = `${base}-${suffix}`;
          heading.id = id;
          usedIds.add(id);
        });
        revision = response.headers.get('ETag');
        if (loaded) window.scrollTo(0, position);
        else if (location.hash) document.getElementById(decodeURIComponent(location.hash.slice(1)))?.scrollIntoView();
        loaded = true;
      }
    }
    status.textContent = 'Автообновление';
    status.title = 'Изменения файла появляются автоматически';
    status.removeAttribute('data-error');
    notice.hidden = true;
  } catch (error) {
    status.textContent = 'Не удалось обновить';
    status.title = error.message;
    status.setAttribute('data-error', '');
    notice.textContent = error.message + '. Проверьте файл и нажмите «Обновить».';
    notice.hidden = false;
  } finally {
    refreshing = false;
    refreshButton.disabled = false;
    refreshButton.classList.remove('is-refreshing');
    refreshButton.removeAttribute('aria-busy');
    refreshTimer = setTimeout(refresh, 600);
  }
}
refresh();
)JS";
