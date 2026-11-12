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

inline constexpr const char* typography_checks = R"JS(
(() => {
  if (!document.querySelector('#files a')) return;
  const groups = ['.toolbar-navigation', '.toolbar-actions'];
  for (const selector of groups) {
    const buttons = [...document.querySelector(selector).querySelectorAll('button,a')]
      .filter(button => button.getBoundingClientRect().width > 0);
    for (let i = 1; i < buttons.length; ++i) {
      const gap = buttons[i].getBoundingClientRect().left - buttons[i-1].getBoundingClientRect().right;
      if (Math.abs(gap - 6) > 1) throw new Error('Uneven toolbar button spacing: ' + gap);
    }
    if (buttons.some(button => Math.abs(button.getBoundingClientRect().height - 34) > 1))
      throw new Error('Toolbar control heights differ');
  }
  const body = getComputedStyle(document.body);
  const context = document.createElement('canvas').getContext('2d');
  context.font = body.fontSize + ' ' + body.fontFamily;
  if (context.measureText('WWWW').width <= context.measureText('iiii').width * 1.5)
    throw new Error('Reader font is not proportional');
  if (parseFloat(body.fontSize) < 16 || parseFloat(getComputedStyle(document.querySelector('#file-search')).fontSize) < 12)
    throw new Error('Reader text is too small');
})();
)JS";

inline constexpr const char* math_checks = R"JS(
(() => {
  const main = document.querySelector('main');
  if (!main.textContent.includes('Native window test')) return;
  const display = main.querySelector('x-equation[type=display]');
  if (!display?.querySelector('.katex-html') || display.querySelector('.katex-error'))
    throw new Error('Aligned expression did not render');
  if (display.querySelectorAll('mtable mtr').length !== 4)
    throw new Error('Aligned rows were not preserved');
  if (!main.querySelector('x-equation:not([type]) .katex'))
    throw new Error('Inline math did not render');
  if (!main.querySelector('.katex-error')) throw new Error('Invalid math was not shown as source');
  const legacy = [...main.querySelectorAll('x-equation')].find(equation => equation.querySelector('annotation')?.textContent === String.raw`d_{\text{standoff}}`);
  if (!legacy?.querySelector('msub')) throw new Error('Escaped Markdown subscript did not render as a subscript');
  const label = [...main.querySelectorAll('x-equation')].find(equation => equation.querySelector('annotation')?.textContent === String.raw`\text{view\_max}_{\text{color}}`);
  if (!label?.querySelector('msub') || !label.textContent.includes('view_max'))
    throw new Error('Literal underscore in a TeX text label was changed');
  const cases = [
    [String.raw`x_0`, String.raw`x_0`],
    [String.raw`d\_{\text{fwd}}`, String.raw`d_{\text{fwd}}`],
    [String.raw`\mathbf{P}\_{\text{rover}}`, String.raw`\mathbf{P}_{\text{rover}}`],
    [String.raw`\text{conf}\_i`, String.raw`\text{conf}_i`],
    [String.raw`\text{rover\_map}`, String.raw`\text{rover\_map}`],
    [String.raw`\texttt{a\_{b}}`, String.raw`\texttt{a\_{b}}`],
    [String.raw`\operatorname{arg\_max}\_i`, String.raw`\operatorname{arg\_max}_i`],
    [String.raw`\verb|x\_i|`, String.raw`\verb|x\_i|`],
    [String.raw`\text{nested {a\_b} and \{c\_d\}}\_i`, String.raw`\text{nested {a\_b} and \{c\_d\}}_i`],
    [String.raw`\begin{aligned}x\_i &= 1 \\ y\_j &= 2\end{aligned}`, String.raw`\begin{aligned}x_i &= 1 \\ y_j &= 2\end{aligned}`]
  ];
  for (const [source, expected] of cases) {
    if (normalizeMathSource(source) !== expected) throw new Error('Math compatibility changed literal text or TeX syntax: ' + source);
  }
  if (main.querySelector('pre .katex')) throw new Error('Code example was rendered as math');
  if (display.getBoundingClientRect().width > main.getBoundingClientRect().width)
    throw new Error('Math overflows the document');
  if (document.fonts.status === 'loaded' && !document.fonts.check('16px KaTeX_Main'))
    throw new Error('Bundled math font did not load');
})();
)JS";

inline constexpr const char* updated_math_check = R"JS(
if (document.querySelector('main').textContent.includes('Updated native document')
    && !document.querySelector('main x-equation .katex'))
  throw new Error('Math did not render after saving the document');
)JS";
