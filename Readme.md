Name: Samiksha Sapkota
Date: March 8, 2026
Environment: Mac , Visual Studio Code
How to compile the project:
Type 'make'
Example of how to run the project:
./oss -n 5 -s 3 -t 5.3 -i 0.5 -f log.txt

Program behaviour:

 I am creating a shared memory which is storing  value for custom/simulated clock which is created by oss.cpp. This clock is accessed by worker.cpp to check the time and terminate itself at the given time(this is given by oss.cpp). Every 0.5 seconds oss.cpp also prints the process table which store data about the process it launched, such as PID,Occupied, Start Sec, Start Nano, End Sec, End Nano and Message sent. oss.cpp send and receive message from worker.cpp. When worker decide to terminate the process, it will send the message to oss.cpp with an integer 0 (i.e terminating) and oss.cpp updates it's PCB and create a new process, but before that it will check if the Launch condition is meet. It is also creating a random time between 1 and t sec and nano sec, which is used to estimate the terminating time of process.

Generative AI used: ChatGPT and Google AI overview:
 Prompt: getting a file name from command line and logging the output in it c++ (Google AI overview)

 Suggestion:
 To get a filename from the command line and log output to it in C++, you need to use the main function's argc and argv parameters, and then use an ofstream to write to the specified file. 
 using the help of an example from there I added the code in my project for writing in log file.

 Prompt:
 missing file in ftok stopped the execution, how to use a touch command before ftok if file not exist, cpp (Google AI overview)

 Suggestion:
 got a suggestion about system(“touch filename”)

 Prompt:
 my start time and endtime for process is same in my processTable, the random number I am generating is not giving me a valid value.(ChatGPT)
 Suggestion:
 I uploaded my code for random time generation,
 turned out I was not considering the case where my nano would be 0 and it was crashing my whole random time generating code. and returning the initial value of zero, that is why my start time and end time were similar. 


