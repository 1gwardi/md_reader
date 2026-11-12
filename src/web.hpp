#pragma once

#include <string_view>

inline constexpr std::string_view stylesheet = R"CSS(
:root { color-scheme: light dark; --bg:#faf9f6; --fg:#27272a; --muted:#71717a; --line:#deded8; --code:#eeeee8; --link:#2563eb; }
@media(prefers-color-scheme:dark) { :root { --bg:#19191c; --fg:#e4e4e7; --muted:#a1a1aa; --line:#38383e; --code:#25252b; --link:#93bbff; } }
* { box-sizing:border-box; }
body { margin:0; background:var(--bg); color:var(--fg); font:17px/1.75 -apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif; overflow-wrap:break-word; }
header { max-width:880px; margin:auto; padding:22px 40px; border-bottom:1px solid var(--line); display:flex; gap:20px; justify-content:space-between; font-size:13px; color:var(--muted); }
#filename { overflow:hidden; text-overflow:ellipsis; white-space:nowrap; }
#status { flex-shrink:0; }
#status[data-error] { color:#dc6565; }
.muted { color:var(--muted); }
.with-explorer { padding-left:260px; }
#explorer { position:fixed; inset:0 auto 0 0; width:260px; padding:24px 14px; overflow-y:auto; border-right:1px solid var(--line); font-size:14px; }
.root-link { display:block; font-weight:650; color:var(--fg); text-decoration:none; padding:0 10px 16px; overflow-wrap:anywhere; }
#breadcrumbs { display:flex; flex-wrap:wrap; gap:4px 8px; padding:0 10px 18px; font-size:12px; }
#breadcrumbs a { color:var(--muted); }
.file-list { list-style:none; margin:0; padding:0; }
.file-list a { display:flex; gap:10px; padding:9px 10px; border-radius:6px; text-decoration:none; color:var(--fg); overflow-wrap:anywhere; line-height:1.5; }
.file-list a:hover, .file-list a[aria-current=page] { background:var(--code); }
.file-kind { flex-shrink:0; width:28px; color:var(--muted); font:11px/2 ui-monospace,monospace; }
#directory-list .file-list a { padding:14px 12px; border-bottom:1px solid var(--line); border-radius:0; }
.empty-list { color:var(--muted); padding:12px 10px; font-size:14px; }
main { max-width:880px; margin:0 auto; padding:32px 40px 100px; }
h1,h2,h3,h4,h5,h6 { line-height:1.3; margin:1.6em 0 .65em; font-weight:650; letter-spacing:-.02em; }
h1 { font-size:2.15em; } h2 { font-size:1.55em; border-bottom:1px solid var(--line); padding-bottom:.35em; }
main > :first-child { margin-top:0; }
p,ul,ol,pre,blockquote,table { margin:0 0 1.2em; }
li > p { margin-bottom:.4em; }
a { color:var(--link); text-underline-offset:3px; }
img { max-width:100%; height:auto; border-radius:6px; }
pre,code { font-family:ui-monospace,SFMono-Regular,Consolas,monospace; font-size:.88em; background:var(--code); border-radius:5px; }
code { padding:.15em .35em; } pre { overflow:auto; padding:18px 22px; line-height:1.6; } pre code { padding:0; font-size:1em; }
blockquote { margin-left:0; padding:0 20px; border-left:3px solid var(--line); color:var(--muted); }
table { display:block; overflow:auto; border-collapse:collapse; }
th,td { padding:9px 16px; border:1px solid var(--line); } th { background:var(--code); }
hr { border:0; border-top:1px solid var(--line); margin:2em 0; }
input[type=checkbox] { margin-right:.5em; }
details { margin:0 0 1.2em; padding:12px 18px; border:1px solid var(--line); border-radius:6px; }
summary { cursor:pointer; font-weight:600; } details[open] > summary { margin-bottom:12px; }
@media(max-width:600px) { body { font-size:16px; } header { padding:16px 20px; } main { padding:24px 20px 64px; } }
@media(max-width:700px) { .with-explorer { padding-left:0; } #explorer { position:static; width:auto; max-height:260px; border-right:0; border-bottom:1px solid var(--line); padding:16px 20px; } }
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
const contentURL = '/__mdread/content' + (directoryMode ? '?path=' + encodeURIComponent(selected) : '');
let revision = '';
let loaded = false;
let lastListing = '';
function fileList(entries) {
  const list = document.createElement('ul');
  list.className = 'file-list';
  for (const entry of entries) {
    const item = document.createElement('li');
    const link = document.createElement('a');
    link.href = entry.url;
    if (entry.path === selected) link.setAttribute('aria-current', 'page');
    const kind = document.createElement('span');
    kind.className = 'file-kind';
    kind.textContent = entry.directory ? 'DIR' : 'MD';
    kind.setAttribute('aria-hidden', 'true');
    link.append(kind, document.createTextNode(entry.name));
    item.append(link);
    list.append(item);
  }
  if (!entries.length) {
    const item = document.createElement('li');
    item.className = 'empty-list';
    item.textContent = 'Здесь нет Markdown-файлов и папок';
    list.append(item);
  }
  return list;
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
async function refreshListing() {
  const response = await fetch('/__mdread/list?path=' + encodeURIComponent(folder), {
    cache: 'no-store', signal: AbortSignal.timeout(4000)
  });
  const data = await response.json();
  if (!response.ok) throw new Error(data.error);
  const listing = JSON.stringify(data.entries);
  if (listing !== lastListing) {
    const position = explorer.scrollTop;
    sidebar.replaceChildren(fileList(data.entries));
    explorer.scrollTop = position;
    directoryList?.replaceChildren(fileList(data.entries));
    lastListing = listing;
  }
}
if (directoryMode) showBreadcrumbs();
async function refresh() {
  try {
    if (directoryMode) await refreshListing();
    if (reading) {
      const response = await fetch(contentURL, {
        headers: revision ? {'If-None-Match': revision} : {},
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
    status.textContent = 'Live · 0.6 s';
    status.removeAttribute('data-error');
  } catch (error) {
    status.textContent = 'Ожидание файла / сервера';
    status.title = error.message;
    status.setAttribute('data-error', '');
  } finally { setTimeout(refresh, 600); }
}
refresh();
)JS";
