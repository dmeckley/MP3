-------------------------------------------------------------------------
Name: 
	Dustin J. Meckley
-------------------------------------------------------------------------
Date: 
	12/10/2015
-------------------------------------------------------------------------
Files Associated: 
	Makefile 

	main.c 	
	manager.c 
	router.c 
	simulator.c
	simulator.h 

	network1.txt 
	network2.txt 
	network3.txt 

	0.rtable
	1.rtable
	2.rtable
	3.rtable
	4.rtable
	5.rtable
	6.rtable
	7.rtable
	8.rtable
	9.rtable

	packets1.txt
	packets2.txt
	packets3.txt

	README.txt 
	design.txt 
-------------------------------------------------------------------------
Description of Execution:
	1) To execute this program you must change directory (cd) to the directory that contains all of the files of this program.  

	2) When you are located at this directory on the commandline interface, then you type in "make" and press enter. It should then create the object files and the executable file so you may want to list directory (ls) command just to make sure that all of the object files and executable file was created.  

	3) After verification that the object files and executable file was created, then you will want to type "./ls-sim network#.txt" or "./ls-sim network# packets#" into the commandline interface replacing # with the network number topology or packets number description file that you would like to execute.  Now press enter.  Program should now execute properly.

	4) To clean-up all of the object files and executable file, then you must type "make clean" into the commandline interface and press enter.  Again, you may want to list directory (ls) command in order to verify that the object files and executable file did indeed go away from the directory.

	5) If you want to run it again, then just repeat the steps above filling in whatever network number topology or packet number description file that you may wish to see.

	6) If you are going to run an execution multiple times in a row, then disregard the make clean phase in step # 4.  
-------------------------------------------------------------------------
Notes Towards the End User (Professor):
	Did not complete part # 4 of the project due to the complexity of it.  This is done at Dr. Warn's request stating that he was dropping it off of the assignment.

	Also, I created my program in each individual steps combining them as I went along; therefore, I have four different directories: part1 is for part1 of MP3, part2 is for part2 of MP3, part3 is for part3 of MP3, and final is the overall program of MP3.  I got everything to work properly but when it came to part3 and the final portions of MP3, I could never get the program to execute with just one network#.txt topology like in parts 1 and 2.  Therefore, executing part 1 & 2 verifies that those portions work and parts 3 and final verified that the trace portion works with the packets#.txt.  If I had a little more time, I might have been able to get them all to work in one program like the expectations that were put forth on me but with the limited of time available to work on it, then this will have to do.  Sorry!  
-------------------------------------------------------------------------
	