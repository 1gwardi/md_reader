# KaTeX 0.19.1

Source: the official [v0.19.1 release](https://github.com/KaTeX/KaTeX/releases/tag/v0.19.1),
`katex.tar.gz`. JavaScript and WOFF2 fonts are unmodified. The stylesheet is
upstream `katex.min.css` with only WOFF/TTF fallback URLs removed; WKWebView
and WebKitGTK support WOFF2. The 20 WOFF2 fonts cover KaTeX's font families.

`LICENSE` is KaTeX's upstream MIT license. `FONTS-LICENSE` is the upstream
[katex-fonts license](https://github.com/KaTeX/katex-fonts/blob/master/LICENSE).

CMake embeds the JavaScript, CSS and fonts in the executable. No CDN, npm,
Python or network connection is required to build or display math.

When updating, replace the release files and licenses, remove the legacy font
fallbacks again, and run HTTP and native window tests. Test `aligned`, inline
math, code examples, invalid expressions and offline font loading.
