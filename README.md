# Mandelbrot-Displayer-using-Perturbation
A simple mandelbrot set displayer using Perturbation theory and double precision for deltas using fragment shader and SSBO

# Building

## Preliminaries

To build, make sure you have:
* OpenGL
* Cmake
* OpenMP
* glfw3

## Hardware Requirments

**HIGHLY** recommended to have a dedicated GPU, it will work with integrated just be ready for massive decreases in performance of your machine

## Building

To build simply build and execute:

```bash
cmake -B build && cmake --build build
build/MandelbrotDisplayer [arguments]
```

# Arguments

For arguments you can choose to select a specific zoom locations by passing 3 arguments for real and imaginary coordinate and scale (width).

Example:
``` bash
build/MandelbrotDisplayer  -1.7499984109937408174900248316242839345282217
233580853461694393097636472584665554041764672708557196273657815 -0.00000000000000165712469295418692325810961981279189026504290
127375760405334498110850956047368308707050735960323397389547038231194872482690340369921750514146922400928554011996123112902000
856666847088788158433995358406779259404221904755 4.784E-10
```

# Controls

There are certain binds that help with controlling the program.

There is zooming both on click and scrolling. And pressing `-` and `=`, `halves` and `doubles` the amount of iterations respectively.

Pressing `s` exports image to the [folder](./img), and pressing `h` exports 8k resolution image of the current position. Due to the way progressive iteration is built you might need high VRAM and RAM amount if you want higher resolution, because SSBO at 16k resolution would result in more than 30GB, while 8K is around 3GB.

Pressing `r` changes t he width and position to the top of the set.

Base amount of iterations is 10 000, and is reached by 500 iterations each update on the current frame.

# Technical notes

Currently only perturbation is done using high precision arithmetic on the cpu. 

$`\epsilon_{n+1}=2z_n\epsilon_n+\epsilon_n^2+\delta`$

At higher zooms it will become cpu bound if you use small enough resolution. At my machine with RTX 4060 mobile it took around 10-20 minutes for 7 million iterations on the cpu and 40-60 minutes to render in 2k resolution. Using smaller resolution time was significantly smaller.

It can be improved using series approximation and cut down from time rendering it, but need to read on that more.

$`\epsilon_{n}=A_n\delta+B_n\delta^2+C_n\delta^3` + ...$

There are more terms that will increase accuracy but I have yet to figure out *how* do i properly skip it, I can simply double performance and skip every second iteration but this is subpar optimization, will read more about it later

Implementing perturbation for multirots has been quite a problem for non-integers powers due to the binomial theorem.

Using taylor series extension of binominal theorem for non-integer n for (a+b)^n. I am still doubtfull about this because while it allows to approximate.
Perturbation for the non-integer powers is fairly terrrible due to the series becoming infinite and while it does converge there are other problems like loss of precision when computing angles for powers of complex numbers due to the builtin GLSL functions not properly supporting double precision floats. Very likely I am getting calculations messed up when I get norm of the complex number and use it for calculations or when angle is not exact.

While i am using z^n=|z|^n(cos(n*Arg(z))+isin(n*Arg(z))), GLSL has no atan, pow, cos and sin for doubles, likely need my own approximation methods to be used here.
Can use series approximation for log, sin and cos so that there is sufficient amount of decimals.
Also, one thing to note, if I get theta of the orbit, I should be able to simply compute it by addition, though it would break rebasing method that might be implemented in the future