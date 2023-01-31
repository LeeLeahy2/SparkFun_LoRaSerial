// C program to implement one side of FIFO
// This side writes first, then reads
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "Sprinkler_Server_Notification.h"

int main(int argc, char **argv)
{
    int bytesWritten;
    int fd_pipe;
    int status;

    status = 0;
    do
    {
        //Open the named pipe (file)
printf ("Calling open %s<br>\n", namedPipe);
        fd_pipe = open(namedPipe, O_WRONLY | O_NONBLOCK);
        if (fd_pipe < 0)
        {
            status = errno;
            printf("ERROR: Failed to open named pipe; %s %s<br>\n", strerror(status), namedPipe);
            break;
        }

        //Notify the sprinkler server that something changed
printf ("Calling write<br>\n");
        bytesWritten = write(fd_pipe, "*", 1);
        if (bytesWritten < 0)
        {
            status = errno;
            printf("ERROR: Failed to write to named pipe: %s<br>\n", strerror(status));
        }

        //Close the pipe
printf ("Calling close<br>\n");
        close(fd_pipe);
    } while (0);
    return status;
}
