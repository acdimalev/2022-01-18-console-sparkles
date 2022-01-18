#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

void sleep_usec(uint64_t usec) {
  clock_nanosleep(CLOCK_MONOTONIC, 0,
    &(struct timespec){usec / 1000000, usec % 1000000 * 1000}, 0);
}

int main(int argc, char **argv)
{ // FIXME: handle dynamic terminal resizing
  // get terminal size
  uint16_t cols, rows;
  { struct winsize winsize;
    ioctl(1, TIOCGWINSZ, &winsize);
    cols = winsize.ws_col;
    rows = winsize.ws_row;
  }

  // set raw terminal mode
  struct termios termios;
  { struct termios raw_termios;
    tcgetattr(1, &termios);
    raw_termios = termios;
    cfmakeraw(&raw_termios);
    tcsetattr(1, TCSAFLUSH, &raw_termios);
  }

  // enable non-blocking input
  fcntl(0, F_SETFL, fcntl(0, F_GETFL) | O_NONBLOCK);

  // enable mouse
  write(0, "\33[?1003h", 8);

  uint8_t grid[64 * 64];

  for (int done = 0; !done;)
  { // FIXME: subtract logic time
    sleep_usec(1000000l / 30);

    // handle input
    struct position {
      uint8_t col, row;
    } path[128];
    int path_n = 0;
    // TODO: keep track of mouse between frames
    // path[path_n++] = (struct position){0, 0};
    { char buf[256];
      ssize_t n = read(0, buf, 256);
      for (char *s = buf; s < buf + n - 5; s++)
      { if (27 == s[0] && '[' == s[1] && 'M' == s[2])
        { int16_t
            b = s[3] - 32
          , x = s[4] - 32
          , y = s[5] - 32
          ;
          path[path_n++] = (struct position){x, y};
          printf("%d %d %d\r\n", b, x, y);
        }
      }
      for (char *s = buf; s < buf + n; s++)
      { if ('q' == s[0])
          done = 1;
        if (27 == s[0])
          s += 5;
      }
    }

    // dissolve
    for (int i = 0; i < 64 * 64; i++)
      grid[i] = grid[i] >= 32 ? grid[i] - 32 : 0;

    // paint
    for (int i = 0; i < path_n; i++)
    { int u = path[i].col >> 1 & 0b00111111;
      int v = path[i].row & 0b00111111;
      grid[v * 64 + u] = 255;
    };

    // render output
    { struct
      { uint8_t l;
        char v[4];
      } texture[] =
        { sizeof(" ") - 1, " "
        , sizeof("░") - 1, "░"
        , sizeof("▒") - 1, "▒"
        , sizeof("▓") - 1, "▓"
        , sizeof("█") - 1, "█"
        };

      char buffer[16384];
      int buffer_n = 0;

      for (int row = 0; row < rows; row++)
      for (int col = 0; col < cols; col++)
      { int u = col >> 1 & 0b00111111;
        int v = row & 0b00111111;

        int x = grid[v * 64 + u];
        x = (x >> 6) + !!x;

        for (int i = 0; i < texture[x].l; i++)
          buffer[buffer_n++] = texture[x].v[i];
      }

      write(1, buffer, buffer_n);
    }
  }

  // disable mouse
  write(0, "\33[?1003l", 8);

  // restore terminal mode
  tcsetattr(1, TCSAFLUSH, &termios);

  return 0;
}
