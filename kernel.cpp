// kernel.cpp
#include <stddef.h>
#include <stdint.h>

static inline void outb(uint16_t port, uint8_t val)
{
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void io_wait(void)
{
    outb(0x80, 0);
}


size_t strlen(const char *str)
{
    size_t len = 0;
    while (str[len])
        len++;
    return len;
}

bool strcmp(const char *s1, const char *s2)
{
    while (*s1 && (*s1 == *s2))
    {
        s1++;
        s2++;
    }
    return *s1 == *s2;
}

void memset(void *dest, char val, size_t count)
{
    char *temp = (char *)dest;
    for (; count != 0; count--)
        *temp++ = val;
}

enum vga_color
{
    VGA_COLOR_BLACK = 0,
    VGA_COLOR_BLUE = 1,
    VGA_COLOR_GREEN = 2,
    VGA_COLOR_CYAN = 3,
    VGA_COLOR_RED = 4,
    VGA_COLOR_MAGENTA = 5,
    VGA_COLOR_BROWN = 6,
    VGA_COLOR_LIGHT_GREY = 7,
    VGA_COLOR_DARK_GREY = 8,
    VGA_COLOR_LIGHT_BLUE = 9,
    VGA_COLOR_LIGHT_GREEN = 10,
    VGA_COLOR_LIGHT_CYAN = 11,
    VGA_COLOR_LIGHT_RED = 12,
    VGA_COLOR_LIGHT_MAGENTA = 13,
    VGA_COLOR_LIGHT_BROWN = 14,
    VGA_COLOR_WHITE = 15,
};

static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;
uint16_t *const VGA_MEMORY = (uint16_t *)0xB8000;

size_t terminal_row;
size_t terminal_column;
uint8_t terminal_color;
uint16_t *terminal_buffer;

void terminal_initialize(void)
{
    terminal_row = 0;
    terminal_column = 0;
    terminal_color = VGA_COLOR_LIGHT_GREY | (VGA_COLOR_BLACK << 4);
    terminal_buffer = VGA_MEMORY;
    for (size_t y = 0; y < VGA_HEIGHT; y++)
    {
        for (size_t x = 0; x < VGA_WIDTH; x++)
        {
            const size_t index = y * VGA_WIDTH + x;
            terminal_buffer[index] = (uint16_t)' ' | (uint16_t)terminal_color << 8;
        }
    }
}

void terminal_scroll()
{
    for (size_t y = 0; y < VGA_HEIGHT - 1; y++)
    {
        for (size_t x = 0; x < VGA_WIDTH; x++)
        {
            terminal_buffer[y * VGA_WIDTH + x] = terminal_buffer[(y + 1) * VGA_WIDTH + x];
        }
    }
    for (size_t x = 0; x < VGA_WIDTH; x++)
    {
        terminal_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = (uint16_t)' ' | (uint16_t)terminal_color << 8;
    }
    terminal_row = VGA_HEIGHT - 1;
}

void terminal_putentryat(char c, uint8_t color, size_t x, size_t y)
{
    const size_t index = y * VGA_WIDTH + x;
    terminal_buffer[index] = (uint16_t)c | (uint16_t)color << 8;
}

void terminal_putchar(char c)
{
    if (c == '\n')
    {
        terminal_column = 0;
        if (++terminal_row == VGA_HEIGHT)
            terminal_scroll();
        return;
    }
    terminal_putentryat(c, terminal_color, terminal_column, terminal_row);
    if (++terminal_column == VGA_WIDTH)
    {
        terminal_column = 0;
        if (++terminal_row == VGA_HEIGHT)
            terminal_scroll();
    }
    uint16_t pos = terminal_row * VGA_WIDTH + terminal_column;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

void terminal_write(const char *data)
{
    for (size_t i = 0; i < strlen(data); i++)
        terminal_putchar(data[i]);
}

void terminal_writeln(const char *data)
{
    terminal_write(data);
    terminal_putchar('\n');
}

void terminal_backspace()
{
    if (terminal_column == 0 && terminal_row > 0)
    {
        terminal_row--;
        terminal_column = VGA_WIDTH;
    }
    if (terminal_column > 0)
    {
        terminal_column--;
        terminal_putentryat(' ', terminal_color, terminal_column, terminal_row);


        uint16_t pos = terminal_row * VGA_WIDTH + terminal_column;
        outb(0x3D4, 0x0F);
        outb(0x3D5, (uint8_t)(pos & 0xFF));
        outb(0x3D4, 0x0E);
        outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
    }
}

char kbd_US[128] = {
    0,
    27,
    '1',
    '2',
    '3',
    '4',
    '5',
    '6',
    '7',
    '8',
    '9',
    '0',
    '-',
    '=',
    '\b',
    '\t', /* Tab */
    'q',
    'w',
    'e',
    'r',
    't',
    'y',
    'u',
    'i',
    'o',
    'p',
    '[',
    ']',
    '\n',
    0, /* Control */
    'a',
    's',
    'd',
    'f',
    'g',
    'h',
    'j',
    'k',
    'l',
    ';',
    '\'',
    '`',
    0, /* Left Shift */
    '\\',
    'z',
    'x',
    'c',
    'v',
    'b',
    'n',
    'm',
    ',',
    '.',
    '/',
    0, /* Right shift */
    '*',
    0,   /* Alt */
    ' ', /* Space bar */
    0,   /* Caps lock */
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0, /* F1 keys */
    0, /* Num lock */
    0, /* Scroll Lock */
    0, /* Home key */
    0, /* Up Arrow */
    0, /* Page Up */
    '-',
    0, /* Left Arrow */
    0,
    0, /* Right Arrow */
    '+',
    0, /* End key */
    0, /* Down Arrow */
    0, /* Page Down */
    0, /* Insert Key */
    0, /* Delete Key */
    0,
    0,
    0,
    0, /* F11 Key */
    0, /* F12 Key */
    0, /* All other keys are undefined */
};

void cmd_reboot()
{
    uint8_t temp;
    asm volatile("cli"); 
    do
    {
        temp = inb(0x64);
        if (temp & 1)
            inb(0x60);
    } while (temp & 2);
    outb(0x64, 0xFE); 
    asm volatile("hlt");
}

void cmd_poweroff()
{
    outb(0x604, 0x2000);
    outb(0xB004, 0x2000);
    terminal_writeln("ACPI Shutdown not available via Ports.");
    terminal_writeln("Please turn off power manually.");
    asm volatile("hlt");
}

void cmd_led(int status)
{
    while ((inb(0x64) & 2) != 0)

    outb(0x60, 0xED); 

    while ((inb(0x64) & 2) != 0)
        ;
    if (status == 1)
        outb(0x60, 0x07); 
    else
        outb(0x60, 0x00); 
}


char cmd_buffer[128];
int cmd_len = 0;

void execute_command()
{
    terminal_writeln(""); 
    if (cmd_len == 0)
        return;

    if (strcmp(cmd_buffer, "about"))
    {
        terminal_color = VGA_COLOR_LIGHT_CYAN;
        terminal_writeln("-------------------------");
        terminal_writeln("  VST Operating System Stable");
        terminal_writeln("  FOR LEARNING ONLY");
        terminal_writeln("-------------------------");
        terminal_color = VGA_COLOR_LIGHT_GREY;
    }
    else if (strcmp(cmd_buffer, "cat"))
    {
        terminal_color = VGA_COLOR_LIGHT_GREEN;
        terminal_writeln("   /\\_/\\  ");
        terminal_writeln("  ( o.o ) ");
        terminal_writeln("   > ^ <  ");
        terminal_writeln("  /  ^  \\ ");
        terminal_color = VGA_COLOR_LIGHT_GREY;
    }
    else if (strcmp(cmd_buffer, "help"))
    {
        terminal_writeln("Available commands:");
        terminal_writeln(" about    - OS info");
        terminal_writeln(" ascii    - Show art");
        terminal_writeln(" clear    - Clear screen");
        terminal_writeln(" led 1    - Turn ON keyboard LEDs");
        terminal_writeln(" led 0    - Turn OFF keyboard LEDs");
        terminal_writeln(" reboot   - Restart system");
        terminal_writeln(" power 0  - Shutdown (QEMU)");
    }
    else if (strcmp(cmd_buffer, "reboot"))
    {
        terminal_writeln("Rebooting...");
        cmd_reboot();
    }
    else if (strcmp(cmd_buffer, "power 0"))
    {
        terminal_writeln("Shutting down...");
        cmd_poweroff();
    }
    else if (strcmp(cmd_buffer, "led 1"))
    {
        cmd_led(1);
        terminal_writeln("LEDs ON");
    }
    else if (strcmp(cmd_buffer, "led 0"))
    {
        cmd_led(0);
        terminal_writeln("LEDs OFF");
    }
    else if (strcmp(cmd_buffer, "clear"))
    {
        terminal_initialize();
    }
    else
    {
        terminal_color = VGA_COLOR_LIGHT_RED;
        terminal_write("Unknown command: ");
        terminal_writeln(cmd_buffer);
        terminal_color = VGA_COLOR_LIGHT_GREY;
    }

    for (int i = 0; i < 128; i++)
        cmd_buffer[i] = 0;
    cmd_len = 0;
    terminal_write("> ");
}


extern "C" void kernel_main(void)
{
    terminal_initialize();


    terminal_color = VGA_COLOR_WHITE;
    terminal_writeln("VST Operating System");
    terminal_writeln("Type 'help' for commands.");
    terminal_writeln("------------------------------");
    terminal_color = VGA_COLOR_LIGHT_GREY;
    terminal_write("> ");


    while (1)
    {
        if (inb(0x64) & 1)
        { 
            uint8_t scancode = inb(0x60);

            if (scancode & 0x80)
            {

            }
            else
            {
                char c = kbd_US[scancode];

                if (c == '\n')
                {
                    execute_command();
                }
                else if (c == '\b')
                {
                    if (cmd_len > 0)
                    {
                        terminal_backspace();
                        cmd_buffer[--cmd_len] = 0;
                    }
                }
                else if (c > 0)
                {
                    if (cmd_len < 79)
                    {
                        terminal_putchar(c);
                        cmd_buffer[cmd_len++] = c;
                        cmd_buffer[cmd_len] = 0;
                    }
                }
            }
        }
    }
}
