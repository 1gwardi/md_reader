void (async () => {
  const assert = (value, message) => { if (!value) throw new Error(message); };
  const wait = ms => new Promise(resolve => setTimeout(resolve, ms));
  const originalFetch = window.fetch;
  try {
    assert(!notice.hidden && document.querySelector('#refresh'), 'Failed initial file removed the application shell');
    while (refreshing || listingBusy) await wait(20);
    clearTimeout(refreshTimer);

    const headings = document.createElement('div');
    headings.innerHTML = '<span id="heading-1"></span><h2>heading-1</h2>' + '<h2>Heading</h2>'.repeat(16000);
    const started = performance.now();
    await assignHeadingIds(headings);
    const ids = [...headings.querySelectorAll('[id]')].map(e => e.id);
    assert(new Set(ids).size === ids.length, 'Heading identifiers collide');
    assert(performance.now() - started < 4000, 'Repeated heading allocation is too slow');

    const formulas = document.createElement('div');
    for (const source of ['x_0', '{'.repeat(10000)+'x'+'}'.repeat(10000), 'y_0']) {
      const equation = document.createElement('x-equation'); equation.textContent = source; formulas.append(equation);
    }
    await renderEquations(formulas);
    assert(formulas.querySelectorAll('.katex').length === 2 && formulas.querySelectorAll('.math-fallback').length === 1, 'A deep formula stopped other equations');
    const render = katex.renderToString;
    try {
      katex.renderToString = () => { throw new RangeError('Synthetic engine failure'); };
      await renderEquations(formulas);
      assert(formulas.querySelectorAll('.math-fallback').length === 3, 'Unexpected KaTeX exception escaped');
    } finally { katex.renderToString = render; }

    const payload = '<h1>Recovered document</h1><x-equation>x_0</x-equation><div id="overlay" style="position:fixed;inset:0;z-index:2147483647;background:red;margin:-200px">Overlay</div>';
    let version = '"review-1"', html = payload;
    window.fetch = async (url, options = {}) => {
      if (String(url).includes('/__mdread/list')) return new Response(JSON.stringify({error:'List unavailable'}), {status:503});
      if (String(url).includes('/__mdread/content')) {
        if (options.headers?.['If-None-Match'] === version) return new Response(null, {status:304});
        return new Response(JSON.stringify({html}), {headers:{ETag:version,'Content-Type':'application/json'}});
      }
      return originalFetch(url, options);
    };
    await refresh(true); clearTimeout(refreshTimer);
    while (listingBusy) await wait(10);
    assert(article.textContent.includes('Recovered document') && notice.hidden, 'List failure prevented content recovery');
    assert(!listingNotice.hidden, 'List failure was not localized to explorer');
    const toolbar = document.querySelector('.app-toolbar').getBoundingClientRect();
    const hit = document.elementFromPoint(toolbar.left + toolbar.width / 2, toolbar.top + toolbar.height / 2);
    assert(hit?.closest('.app-toolbar'), 'Document CSS covered trusted toolbar');
    const first = article.firstChild;
    await refresh(); clearTimeout(refreshTimer);
    assert(article.firstChild === first, 'Unchanged version rebuilt the DOM');

    version = '"review-too-large"'; html = '<br>'.repeat(40001);
    await refresh(true); clearTimeout(refreshTimer);
    assert(!notice.hidden && article.firstChild === first, 'Oversized markup was not rejected safely');
    await refresh(); clearTimeout(refreshTimer);
    assert(!notice.hidden && article.firstChild === first, 'Rejected revision was retried or its error was lost');

    entries = Array.from({length:50000}, (_,i) => ({name:'Item '+i,searchName:'item '+i,path:i+'.md',url:'/files/'+i+'.md',directory:false}));
    search.value = ''; listPage = 0; displayListing();
    assert(sidebar.querySelectorAll('li').length <= 100, 'Sidebar created too many rows');
    if (directoryList) assert(directoryList.querySelectorAll('li').length <= 100, 'Overview created too many rows');
    const before = sidebar.firstChild;
    for (const value of ['i','it','item','item 4','item 49']) { search.value = value; search.dispatchEvent(new Event('input')); }
    assert(sidebar.firstChild === before, 'Large search was not debounced');
    await wait(220);
    assert(sidebar.firstChild !== before && [...sidebar.querySelectorAll('.entry-name')].every(e=>e.textContent.toLowerCase().includes('item 49')), 'Latest search result was not displayed');
    assert(sidebar.querySelectorAll('*').length < 1500, 'List DOM budget exceeded');
    window.__reviewResult = 'passed';
  } catch (error) { window.__reviewResult = 'FAILED: ' + error.message; }
  finally { window.fetch = originalFetch; clearTimeout(refreshTimer); }
})();
