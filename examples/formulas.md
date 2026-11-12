# Формулы и координаты ровера

Формула внутри текста: $c = \sqrt{a^2 + b^2}$.

## Переход к координатам NED

$$
\begin{aligned}
d_{\text{fwd}} &= X_{\text{rover}} - X_{\text{start}} \\
d_{\text{left}} &= Y_{\text{rover}} - Y_{\text{start}} \\
X_{\text{ned}} &= x_0 + d_{\text{fwd}} \cdot \cos(yaw_0) + d_{\text{left}} \cdot \sin(yaw_0) \\
Y_{\text{ned}} &= y_0 + d_{\text{fwd}} \cdot \sin(yaw_0) - d_{\text{left}} \cdot \cos(yaw_0)
\end{aligned}
$$

В `aligned` строки разделяются двумя обратными слешами (`\\`), а `&`
задаёт точку выравнивания. Длинную формулу можно прокручивать по горизонтали.

## Другие примеры

$$
\int_0^1 x^2\,dx = \frac{1}{3}
$$

$$
\begin{pmatrix}
\cos\theta & -\sin\theta \\
\sin\theta & \cos\theta
\end{pmatrix}
$$

Исходный код формулы тоже можно показывать:

```tex
$$ E = mc^2 $$
```

Цена с экранированным долларом: \$100. Измени формулу и сохрани файл — результат
обновится автоматически.
