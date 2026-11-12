# HTML внутри Markdown

Обычный Markdown можно смешивать с HTML: **жирный текст**, H<sub>2</sub>O,
x<sup>2</sup>, <mark>выделение</mark> и перенос<br>строки.

<p style="color: #2563eb; font-size: 20px;">Этот абзац оформлен встроенным CSS.</p>

<details>
<summary>Нажми, чтобы раскрыть</summary>

Внутри можно использовать Markdown, если отделить его от HTML-тегов пустыми строками.

- Первый пункт
- Второй пункт с **выделением**

</details>

<h2 id="html-table">HTML-таблица</h2>

<table>
  <thead><tr><th>Возможность</th><th>Пример</th></tr></thead>
  <tbody>
    <tr><td>Подстрочный текст</td><td>H<sub>2</sub>O</td></tr>
    <tr><td>Надстрочный текст</td><td>x<sup>2</sup></td></tr>
  </tbody>
</table>

<a href="#html-table">Перейти к HTML-таблице</a>

При запуске `mdread examples` можно открыть <a href="demo.md">соседний документ</a>.

## HTML как исходный код

В блоке кода теги показываются текстом:

```html
<details>
  <summary>Заголовок</summary>
  <p>Содержимое</p>
</details>
```

Измени любой HTML-абзац и сохрани файл — содержимое окна обновится автоматически.
