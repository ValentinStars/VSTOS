// kernel.cpp
// так, ну погнали. Это сердце нашей операционки.
// в начале стандартно подключаем типы, чтобы не гадать, сколько байт в инте
#include <stddef.h>
#include <stdint.h>

// это наши "руки" для работы с портами, процессор общается с железками через них
// вот эта штука шлет байт в порт, например, скомандовать клавиатуре мигнуть лампочкой
static inline void outb(uint16_t port, uint8_t val)
{
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

// а эта наоборот - забирает байт. так мы узнаем, какую клавишу нажали
static inline uint8_t inb(uint16_t port)
{
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// маленькая задержка, иногда железо слишком медленное для проца
static inline void io_wait(void)
{
    outb(0x80, 0);
}

// считаем длину строки, пока нет стандартной библиотеки, пишем всё сами
size_t strlen(const char *str)
{
    size_t len = 0;
    while (str[len])
        len++;
    return len;
}

// сравниваем две строки: чтобы понимать, какую команду ввел юзер
bool strcmp(const char *s1, const char *s2)
{
    while (*s1 && (*s1 == *s2))
    {
        s1++;
        s2++;
    }
    return *s1 == *s2;
}

// просто забиваем кусок памяти нужным символом
void memset(void *dest, char val, size_t count)
{
    char *temp = (char *)dest;
    for (; count != 0; count--)
        *temp++ = val;
}

// таблица цветов для VGA, стандартные 16 цветов из эпохи DOS
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

// размеры текстового экрана. почти везде по дефолту 80 на 25
// 0xB8000 это волшебный адрес, куда надо писать буквы, чтобы они появились на мониторе
static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;
uint16_t *const VGA_MEMORY = (uint16_t *)0xB8000;

// состояние нашего "терминала": где сейчас курсор и какой цвет текста
size_t terminal_row;
size_t terminal_column;
uint8_t terminal_color;
uint16_t *terminal_buffer;

// чистим экран при запуске и ставим курсор в начало
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
            // забиваем всё пробелами
            terminal_buffer[index] = (uint16_t)' ' | (uint16_t)terminal_color << 8;
        }
    }
}

// если дошли до низа экрана, надо всё сдвинуть вверх
void terminal_scroll()
{
    for (size_t y = 0; y < VGA_HEIGHT - 1; y++)
    {
        for (size_t x = 0; x < VGA_WIDTH; x++)
        {
            terminal_buffer[y * VGA_WIDTH + x] = terminal_buffer[(y + 1) * VGA_WIDTH + x];
        }
    }
    // последнюю строчку чистим
    for (size_t x = 0; x < VGA_WIDTH; x++)
    {
        terminal_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = (uint16_t)' ' | (uint16_t)terminal_color << 8;
    }
    terminal_row = VGA_HEIGHT - 1;
}

// вспомогательная штука, чтобы положить символ в конкретную точку экрана
void terminal_putentryat(char c, uint8_t color, size_t x, size_t y)
{
    const size_t index = y * VGA_WIDTH + x;
    terminal_buffer[index] = (uint16_t)c | (uint16_t)color << 8;
}

// основная функция печати символа, она обрабатывает перевод строки и двигает курсор
void terminal_putchar(char c)
{
    if (c == '\n') // если встретили энтер, прыгаем на новую строку
    {
        terminal_column = 0;
        if (++terminal_row == VGA_HEIGHT)
            terminal_scroll();
        return;
    }
    
    // рисуем символ
    terminal_putentryat(c, terminal_color, terminal_column, terminal_row);
    
    // двигаем координату вправо
    if (++terminal_column == VGA_WIDTH)
    {
        terminal_column = 0;
        if (++terminal_row == VGA_HEIGHT)
            terminal_scroll();
    }
    
    // тут магия, говорим видеокарте передвинуть мигающую палочку вслед за текстом
    uint16_t pos = terminal_row * VGA_WIDTH + terminal_column;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

// просто печатаем строку целиком
void terminal_write(const char *data)
{
    for (size_t i = 0; i < strlen(data); i++)
        terminal_putchar(data[i]);
}

// то же самое, но в конце добавляем переход на новую строку
void terminal_writeln(const char *data)
{
    terminal_write(data);
    terminal_putchar('\n');
}

// стираем последний символ. нужно для работы Backspac
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

        // не забываем вернуть мигающий курсор назад
        uint16_t pos = terminal_row * VGA_WIDTH + terminal_column;
        outb(0x3D4, 0x0F);
        outb(0x3D5, (uint8_t)(pos & 0xFF));
        outb(0x3D4, 0x0E);
        outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
    }
}

// карта клавиш, железо присылает номер кнопки, а мы превращаем его в букву
char kbd_US[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8',	'9', '0', '-', '=', '\b',	
    '\t', 'q', 'w', 'e', 'r',	't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',	
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',	'\'', '`', 0,		
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',   0, '*', 0, ' ',	
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '-', 0, 0, 0, '+', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

// жесткий метод перезагрузки через контроллер клавиатуры
void cmd_reboot()
{
    uint8_t temp;
    asm volatile("cli"); // вырубаем прерывания, чтобы ничего не мешало
    do
    {
        temp = inb(0x64); // ждем, пока контроллер освободится
        if (temp & 1)
            inb(0x60);
    } while (temp & 2);
    outb(0x64, 0xFE); // шлем команду на ресет
    asm volatile("hlt");
}

// попытка выключить комп. работает в эмуляторах типа QEMU
void cmd_poweroff()
{
    outb(0x604, 0x2000);
    outb(0xB004, 0x2000);
    terminal_writeln("ACPI Shutdown not available via Ports.");
    terminal_writeln("Please turn off power manually.");
    asm volatile("hlt");
}

// управление лампочками на клаве (CapsLock, NumLock и т.д.)
void cmd_led(int status)
{
    while ((inb(0x64) & 2) != 0)
        ;
    outb(0x60, 0xED); // говорим, что сейчас будем менять состояние диодов

    while ((inb(0x64) & 2) != 0)
        ;
    if (status == 1)
        outb(0x60, 0x07); // зажечь всё
    else
        outb(0x60, 0x00); // погасить всё
}

// буфер, куда мы записываем то, что печатает юзер, пока он не нажмет энтер
char cmd_buffer[128];
int cmd_len = 0;

// наш обработчик команд это простейший парсер
void execute_command()
{
    terminal_writeln(""); 
    if (cmd_len == 0)
        return;

    // проверяем, что там ввел пользователь
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

    // после выполнения чистим буфер для новой команды
    for (int i = 0; i < 128; i++)
        cmd_buffer[i] = 0;
    cmd_len = 0;
    terminal_write("> ");
}

// точка входа, именно сюда GRUB передаст управление
extern "C" void kernel_main(void)
{
    // стартуем экран
    terminal_initialize();

    // приветствие
    terminal_color = VGA_COLOR_WHITE;
    terminal_writeln("VST Operating System");
    terminal_writeln("Type 'help' for commands.");
    terminal_writeln("------------------------------");
    terminal_color = VGA_COLOR_LIGHT_GREY;
    terminal_write("> ");
    
    // основной цикл, тут и живет система
    while (1)
    {
        // опрашиваем порт клавиатуры на нажатие чего либо
        if (inb(0x64) & 1)
        { 
            uint8_t scancode = inb(0x60);

            // если старший бит 1, значит клавишу отпустили, нам это пока не интересно
            if (scancode & 0x80)
            {
            }
            else
            {
                // получаем символ из нашей таблицы
                char c = kbd_US[scancode];

                if (c == '\n') // нажали энтер - выполняем
                {
                    execute_command();
                }
                else if (c == '\b') // нажали бакспейс - стираем
                {
                    if (cmd_len > 0)
                    {
                        terminal_backspace();
                        cmd_buffer[--cmd_len] = 0;
                    }
                }
                else if (c > 0) // обычная буква - пишем в буфер и на экран
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
