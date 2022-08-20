#include <zephyr.h>

int main(void)
{
    while(1){
        printk("Hello World!\n");
        k_msleep(1000);
    }
} /* main */