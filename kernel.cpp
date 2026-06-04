extern "C" void kernel_main() {
    volatile unsigned short* vga_buffer = (volatile unsigned short*)0xB8000;
    const char* message = "GitOS Core initialized. Waiting for input...";
    
    for(int i = 0; i < 80 * 25; i++) {
        vga_buffer[i] = (unsigned short)' ' | (0x07 << 8);
    }
    
    int index = 0;
    while(message[index] != '\0') {
        vga_buffer[index] = (unsigned short)message[index] | (0x07 << 8);
        index++;
    }
}
