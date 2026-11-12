#pragma once

inline constexpr const char* resize_checks = R"JS(
(() => {
  if (!document.querySelector('#files a')) return;
  const handle = document.querySelector('#sidebar-resizer');
  const width = () => Math.round(document.querySelector('#explorer').getBoundingClientRect().width);
  const press = key => handle.dispatchEvent(new KeyboardEvent('keydown', {key, bubbles:true}));
  const dragTo = target => {
    const start = width();
    handle.dispatchEvent(new PointerEvent('pointerdown', {pointerId:42, button:0, clientX:start, bubbles:true}));
    window.dispatchEvent(new PointerEvent('pointermove', {pointerId:42, clientX:target, bubbles:true}));
    window.dispatchEvent(new PointerEvent('pointerup', {pointerId:42, clientX:target, bubbles:true}));
  };
  dragTo(320);
  if (width() !== 320 || handle.getAttribute('aria-valuenow') !== '320') throw new Error('Sidebar drag failed');
  press('ArrowRight');
  if (width() !== 330) throw new Error('Sidebar keyboard resize failed');
  dragTo(9000);
  if (width() > 480 || document.documentElement.scrollWidth > innerWidth) throw new Error('Sidebar maximum failed');
  press('Home');
  if (width() !== 190) throw new Error('Sidebar minimum failed');
  handle.dispatchEvent(new MouseEvent('dblclick', {bubbles:true}));
  if (width() !== 250) throw new Error('Sidebar reset failed');
  dragTo(320);
  if (document.body.classList.contains('resizing-sidebar')) throw new Error('Sidebar drag did not end');
})();
)JS";

inline constexpr const char* persisted_width_check = R"JS(
if (document.querySelector('#files a') && Math.round(document.querySelector('#explorer').getBoundingClientRect().width) !== 320)
  throw new Error('Sidebar width did not survive navigation');
)JS";
