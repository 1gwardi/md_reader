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
@media(max-width:600px) { body { font-size:16px; } header { padding:16px 20px; } main { padding:24px 20px 64px; } }
)CSS";

inline constexpr std::string_view browser_script = R"JS(
const article = document.querySelector('main');
const status = document.querySelector('#status');
let revision = '';
let loaded = false;
async function refresh() {
  try {
    const response = await fetch('/__mdread/content', {
      headers: revision ? {'If-None-Match': revision} : {},
      cache: 'no-store', signal: AbortSignal.timeout(4000)
    });
    if (response.status !== 304) {
      const data = await response.json();
      if (!response.ok) throw new Error(data.error);
      const position = window.scrollY;
      article.innerHTML = data.html;
      const usedIds = new Set();
      article.querySelectorAll('h1,h2,h3,h4,h5,h6').forEach(heading => {
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
