# sway-resize

`sway-resize` is a utility to help resize windows visually on Sway with the use of guidelines.

[demo.webm](https://github.com/user-attachments/assets/dd4bd7a6-8e15-4b76-a61b-9ebe7b247d61)

## Usage

```
sway-resize -g GUIDELINES
```

`GUIDE_LINES` is a list of guidelines, each item has the `<symbol>:<direction>:<resize value>` format, e.g. `a:h:+50` will create a guideline with the symbol `a`, the horizontal direction, and with a relative increase in size of 50px. `<direction>` can be either `h` or `v` for the horizontal and vertical directions respectively. `<resize value>` can be on of the following format:

| Format | Example | Description |
| --- | --- | --- |
| `x` | `250` | Set the size to 250px. |
| `x%` | `50%` | Set the size to 50% of the available space. |
| `+x` | `+50` | Increase the size by 50px. |
| `+x%` | `+25%` | Increase the size by 25%. |
| `-x` | `-50` | Decrease the size by 50px. |
| `-x%` | `-25%` | Decrease the size by 25%. |

### Example

```bash
sway-resize -g "a:h:25% b:h:33% c:h:50% d:h:66% k:h:75% l:h:85% e:v:25% f:v:33% g:v:50% h:v:66% i:v:75% j:v:85%"
```

## Installation

### From sources

The program can be built from sources with:

```bash
meson build
meson compile -C build
```

## Bindings

Here's an example of binding:

```
bindsym $mod+r exec sway-resize -g 'a:h:+32 b:h:+64 c:h:+128 d:h:+256 e:h:+512 f:v:+32 g:v:+64 h:v:+128 i:v:+256 j:v:+512 k:h:-32 l:h:-64 m:h:-128 n:h:-256 o:h:-512 p:v:-32 q:v:-64 l:v:-128 m:v:-256 n:v:-512'
```

## Dependencies

- [`xkbcommon`](https://xkbcommon.org)
- [`cairo`](https://cairographics.org)
- [`wayland`](https://wayland.freedesktop.org)
- [`wayland-protocols`](https://gitlab.freedesktop.org/wayland/wayland-protocols)
- [`jansson`](http://www.digip.org/jansson)
