#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <time.h>
#include "../address_map_arm.h"

enum Direction {Front, Right, Back, Left} facingDir;

/* Prototypes for functions used to access physical memory addresses */
int open_physical (int);
void * map_physical (int, unsigned int, unsigned int);
void close_physical (int);
int unmap_physical (void *, unsigned int);
void dr_fwd_preset(volatile int * PWM0_ptr, volatile int * PWM1_ptr);
float dr_fwd_till_obstacle(volatile int * ULTRA0_ptr, volatile int * PWM0_ptr, volatile int * PWM1_ptr);
float dr_bwd_till_obstacle(volatile int * ULTRA2_ptr, volatile int * PWM0_ptr, volatile int * PWM1_ptr);
void tr_left90(volatile int * PWM0_ptr, volatile int * PWM1_ptr);
void tr_right90(volatile int * PWM0_ptr, volatile int * PWM1_ptr);

int main(void)
{
    volatile int * PWM0_ptr;   // virtual address pointer to servopwm0
    volatile int * PWM1_ptr;   // virtual address pointer to servopwm1
    volatile int * ULTRA0_ptr; // to ultrasonic sensor 0
    volatile int * ULTRA1_ptr; // to ultrasonic sensor 1
    volatile int * ULTRA2_ptr; // to ultrasonic sensor 2
    volatile int * ULTRA3_ptr; // to ultrasonic sensor 3

    int fd = -1;               // used to open /dev/mem for access to physical addresses
    void *LW_virtual;          // used to map physical addresses for the light-weight bridge

    // Create virtual memory access to the FPGA light-weight bridge
    if ((fd = open_physical (fd)) == -1)
       return (-1);
    if ((LW_virtual = map_physical (fd, LW_BRIDGE_BASE, LW_BRIDGE_SPAN)) == NULL)
       return (-1);

    // Set virtual address pointer to I/O port
    PWM0_ptr = (unsigned int *) (LW_virtual + PWM0_BASE);
    PWM1_ptr = (unsigned int *) (LW_virtual + PWM1_BASE);
	
    ULTRA0_ptr = (unsigned int *) (LW_virtual + ULTRA0_BASE);   // fonrt sensor
    ULTRA1_ptr = (unsigned int *) (LW_virtual + ULTRA1_BASE);   // right sensor
    ULTRA2_ptr = (unsigned int *) (LW_virtual + ULTRA2_BASE);   // rear sensor
    ULTRA3_ptr = (unsigned int *) (LW_virtual + ULTRA3_BASE);   // left sensor

    // X is the horizontal axis, Y is the vertical axis. Let our primary goal be to move the robot FORWARD by a certain distance
    float distanceInX = 200;
    float distanceInY = 0;
    float distanceDriven = 0;
    /*
                        left/3
                          Y
                          |
                          |
                          |
    rear/2 ---------------|--------------- X front/0
                          |
                          |
                          |
                        right/1
    */
    // allow some error
    while(abs(distanceInX) > 15 || abs(distanceInY) > 15) {
        printf("distanceInX: %f, distanceInY: %f\n", distanceInX, distanceInY);
        // prioritize the direction
        if ( abs(distanceInX) > abs(distanceInY) ) {
            // sensor feedback low: no obstacle, hight: obstacle sensed
            // check if front is clear
            printf("test1\n");
            if ( *ULTRA0_ptr == 0 ) {
                printf("test2, X: %f, Y: %f\n", distanceInX, distanceInY);
                // more to go, target in +X direction
                if ( distanceInX > 0 ) {
                    printf("test3, dir: %d, ultra3: %d\n", facingDir, *ULTRA3_ptr);
                    if ( facingDir == Right && *ULTRA3_ptr == 0 ) {
                        tr_left90(PWM0_ptr, PWM1_ptr);
                        facingDir = Front;
                        printf("test4, %d\n", facingDir);
                    } else if ( facingDir == Back && *ULTRA2_ptr == 0 ) {
                        tr_right90(PWM0_ptr, PWM1_ptr);
                        tr_right90(PWM0_ptr, PWM1_ptr);
                        facingDir = Front;
                    } else if ( facingDir == Left && *ULTRA1_ptr == 0 ) {
                        tr_right90(PWM0_ptr, PWM1_ptr);
                        facingDir = Front;
                    } // else { do nothing }
                // robot went beyond the target, target in -X direction
                } else {
                    if ( facingDir == Right && *ULTRA1_ptr == 0 ) {
                        tr_right90(PWM0_ptr, PWM1_ptr);
                        facingDir = Back;
                    } else if ( facingDir == Front && *ULTRA2_ptr == 0 ) {
                        tr_right90(PWM0_ptr, PWM1_ptr);
                        tr_right90(PWM0_ptr, PWM1_ptr);
                        facingDir = Back;
                    } else if ( facingDir == Left && *ULTRA3_ptr == 0 ) {
                        tr_left90(PWM0_ptr, PWM1_ptr);
                        facingDir = Back;
                    } // else {do nothing}
                }
                // drive forward till obstacle is encountered or the max time threshold is reached, report the distance traveled
                distanceDriven = dr_fwd_till_obstacle(ULTRA0_ptr, PWM0_ptr, PWM1_ptr);
                // Update distances accordingly
                switch (facingDir) {
                    case Front:
                        distanceInX -= distanceDriven; printf("Facing: F\n");
                        break;
                    case Right:
                        distanceInY -= distanceDriven; printf("Facing: R\n");
                        break;
                    case Back:
                        distanceInX += distanceDriven; printf("Facing: B\n");
                        break;
                    case Left:
                        distanceInY += distanceDriven; printf("Facing: L\n");
                        break;
                    default: printf("Error: Illegal facing direction");
                        return 1;
                }
            // obstacle detected in front, check if right side is clear
            } else if ( *ULTRA1_ptr == 0 ) {
                tr_right90(PWM0_ptr, PWM1_ptr);
                facingDir++;
                // loop back to facing front
                if ( facingDir == Left + 1 ) {
                    facingDir = Front;
                } else if ( facingDir > Left + 1 ) {
                    printf("Facing direction out of highest range");
                    return 1;
                }
            // obstacle detected in front & right, check if left side is clear
            } else if ( *ULTRA3_ptr == 0 ) {
                tr_left90(PWM0_ptr, PWM1_ptr);
                facingDir--;
                // loop back to facing left
                if ( facingDir == Front - 1 ) {
                    facingDir = Left;
                } else if ( facingDir < Front - 1 ) {
                    printf("Facing direction out of lowest range");
                    return 1;
                }
            // obstacle detected in front & right & left, check if rear side is clear
            } else if ( *ULTRA2_ptr == 0 ) {
                // drive backward till obstacle is encountered or the max time threshold is reached, report the distance traveled
                distanceDriven = dr_bwd_till_obstacle(ULTRA2_ptr, PWM0_ptr, PWM1_ptr);
                // Update distances accordingly
                switch (facingDir) {
                    case Front:
                        distanceInX += distanceDriven; printf("Facing: F\n");
                        break;
                    case Right:
                        distanceInY += distanceDriven; printf("Facing: R\n");
                        break;
                    case Back:
                        distanceInX -= distanceDriven; printf("Facing: B\n");
                        break;
                    case Left:
                        distanceInY -= distanceDriven; printf("Facing: L\n");
                        break;
                    default: printf("Error: Illegal facing direction");
                        return 1;
                }
            // surrounded by obstacles
            } else {
                printf("Surrounded by obstacles, SOS");
                return 2;
            }
        // prioritize the direction
        } else {
            /* sensor feedback low: no obstacle, hight: obstacle sensed */
            if ( *ULTRA0_ptr == 0 ) {
                // target in +Y direction
                if ( distanceInY > 0 ) {
                    if ( facingDir == Front && *ULTRA1_ptr == 0 ) {
                        tr_right90(PWM0_ptr, PWM1_ptr);
                        facingDir = Right;
                    } else if ( facingDir == Back && *ULTRA3_ptr == 0 ) {
                        tr_left90(PWM0_ptr, PWM1_ptr);
                        facingDir = Right;
                    } else if ( facingDir == Left && *ULTRA2_ptr == 0 ) {
                        tr_right90(PWM0_ptr, PWM1_ptr);
                        tr_right90(PWM0_ptr, PWM1_ptr);
                        facingDir = Right;
                    } // else { do nothing }
                // target in -Y direction
                } else {
                    if ( facingDir == Front && *ULTRA3_ptr == 0 ) {
                        tr_left90(PWM0_ptr, PWM1_ptr);
                        facingDir = Left;
                    } else if ( facingDir == Right && *ULTRA2_ptr == 0 ) {
                        tr_right90(PWM0_ptr, PWM1_ptr);
                        tr_right90(PWM0_ptr, PWM1_ptr);
                        facingDir = Left;
                    } else if ( facingDir == Back && *ULTRA1_ptr == 0 ) {
                        tr_right90(PWM0_ptr, PWM1_ptr);
                        facingDir = Left;
                    } // else {do nothing}
                }
                // drive forward till obstacle is encountered or the max time threshold is reached, report the distance traveled
                distanceDriven = dr_fwd_till_obstacle(ULTRA0_ptr, PWM0_ptr, PWM1_ptr);
                // Update distances accordingly
                switch (facingDir) {
                    case Front:
                        distanceInX -= distanceDriven; printf("Facing: F\n");
                        break;
                    case Right:
                        distanceInY -= distanceDriven; printf("Facing: R\n");
                        break;
                    case Back:
                        distanceInX += distanceDriven; printf("Facing: B\n");
                        break;
                    case Left:
                        distanceInY += distanceDriven; printf("Facing: L\n");
                        break;
                    default: printf("Error: Illegal facing direction");
                        return 1;
                }
            // obstacle detected in front, check if right side is clear
            } else if ( *ULTRA1_ptr == 0 ) {
                tr_right90(PWM0_ptr, PWM1_ptr);
                facingDir++;
                // loop back to facing front
                if ( facingDir == Left + 1 ) {
                    facingDir = Front;
                } else if ( facingDir > Left + 1 ) {
                    printf("Facing direction out of highest range");
                    return 1;
                }
            // obstacle detected in front & right, check if left side is clear
            } else if ( *ULTRA3_ptr == 0 ) {
                tr_left90(PWM0_ptr, PWM1_ptr);
                facingDir--;
                // loop back to facing left
                if ( facingDir == Front - 1 ) {
                    facingDir = Left;
                } else if ( facingDir < Front - 1 ) {
                    printf("Facing direction out of lowest range");
                    return 1;
                }
            // obstacle detected in front & right & left, check if rear side is clear
            } else if ( *ULTRA2_ptr == 0 ) {
                // drive backward till obstacle is encountered or the max time threshold is reached, report the distance traveled
                distanceDriven = dr_bwd_till_obstacle(ULTRA2_ptr, PWM0_ptr, PWM1_ptr);
                // Update distances accordingly
                switch (facingDir) {
                    case Front:
                        distanceInX += distanceDriven; printf("Facing: F\n");
                        break;
                    case Right:
                        distanceInY += distanceDriven; printf("Facing: R\n");
                        break;
                    case Back:
                        distanceInX -= distanceDriven; printf("Facing: B\n");
                        break;
                    case Left:
                        distanceInY -= distanceDriven; printf("Facing: L\n");
                        break;
                    default: printf("Error: Illegal facing direction");
                        return 1;
                }
            // surrounded by obstacles
            } else {
                printf("Surrounded by obstacles, SOS");
                return 2;
            }
        }
    }

    unmap_physical (LW_virtual, LW_BRIDGE_SPAN);   // release the physical-memory mapping
    close_physical (fd);   // close /dev/mem
    return 0;
}

void dr_fwd_preset(volatile int * PWM0_ptr, volatile int * PWM1_ptr)
{
    *PWM0_ptr = 200000;
    *PWM1_ptr = 100000;
    usleep(1000000);
    *PWM0_ptr = 150000;
    *PWM1_ptr = 150000;
}

float dr_fwd_till_obstacle(volatile int * ULTRA0_ptr, volatile int * PWM0_ptr, volatile int * PWM1_ptr) {
    time_t start, present;
    float elapse, distanceDriven;
    *PWM0_ptr = 162000;
    *PWM1_ptr = 100000;
    start = clock();    // start of timer
    present = clock();  // present time
    elapse = ( present - start ) / CLOCKS_PER_SEC;
    // while there is no obstacle in front, and time elapsed is smaller than 2sec
    while ( *ULTRA0_ptr == 0 && elapse < 2 ){
        present = clock();
        elapse = ( present - start ) / CLOCKS_PER_SEC;
    }
    *PWM0_ptr = 150000;
    *PWM1_ptr = 150000;

    distanceDriven = elapse * 15;       // speed of the robot is approx. 15 cm/s

    return distanceDriven;
}

float dr_bwd_till_obstacle(volatile int * ULTRA2_ptr, volatile int * PWM0_ptr, volatile int * PWM1_ptr) {
    time_t start, present;
    float elapse, distanceDriven;
    *PWM0_ptr = 100000;
    *PWM1_ptr = 200000;
    start = clock();    // start of timer
    present = clock();  // present time
    elapse = ( present - start ) / CLOCKS_PER_SEC;
    // while there is no obstacle in back
    while ( *ULTRA2_ptr == 0 && elapse < 2 ){
        present = clock();
        elapse = ( present - start ) / CLOCKS_PER_SEC;
    }
    *PWM0_ptr = 150000;
    *PWM1_ptr = 150000;

    distanceDriven = elapse * 15;       // speed of the robot is approx. 15 cm/s

    return distanceDriven;
}

void tr_left90(volatile int * PWM0_ptr, volatile int * PWM1_ptr)
{
    *PWM0_ptr = 100000;
    *PWM1_ptr = 100000;
    usleep(1000000);
    *PWM0_ptr = 150000;
    *PWM1_ptr = 150000;
    usleep(150000);
}

void tr_right90(volatile int * PWM0_ptr, volatile int * PWM1_ptr)
{
    *PWM0_ptr = 200000;
    *PWM1_ptr = 200000;
    usleep(920000);
    *PWM0_ptr = 150000;
    *PWM1_ptr = 150000;
    usleep(150000);
}

// Open /dev/mem, if not already done, to give access to physical addresses
int open_physical (int fd)
{
    if (fd == -1)
        if ((fd = open( "/dev/mem", (O_RDWR | O_SYNC))) == -1)
        {
             printf ("ERROR: could not open \"/dev/mem\"...\n");
            return (-1);
        }
    return fd;
}

// Close /dev/mem to give access to physical addresses
void close_physical (int fd)
{
    close (fd);
}

/*
 * Establish a virtual address mapping for the physical addresses starting at base, and
 * extending by span bytes.
 */
void* map_physical(int fd, unsigned int base, unsigned int span)
{
    void *virtual_base;

    // Get a mapping from physical addresses to virtual addresses
    virtual_base = mmap (NULL, span, (PROT_READ | PROT_WRITE), MAP_SHARED, fd, base);
    if (virtual_base == MAP_FAILED)
    {
       printf ("ERROR: mmap() failed...\n");
       close (fd);
       return (NULL);
    }
    return virtual_base;
}

/*
 * Close the previously-opened virtual address mapping
 */
int unmap_physical(void * virtual_base, unsigned int span)
{
    if (munmap (virtual_base, span) != 0)
    {
       printf ("ERROR: munmap() failed...\n");
       return (-1);
    }
    return 0;
}
