# Project 1: NPShell

- Introduction: In this project, you are asked to design a shell with special piping mechanisms.

- Please refer to **NP_Project1_Spec.pdf** for detailed description.

  

**Additional Information:**

- You should run your code on the workstation (nplinux[1-12].cs.nctu.edu.tw).

- - Open file limit: 1024
  - Processes limit: 512

- You should create a repo named **<Student ID>_np_project1** in bitbucket and commit at least 5 times before demo.

- **Lex & Yacc are NOT allowed**

- Number pipe |N and !N will only appear at the end of the line.
- If you only let one child process run at a time, you will not be able to deal with large data exceeding the buffer limit of the pipe.

For example: % cat large_file.txt | number

If you only let one child process run at a time, **cat** wouldn't be able to write to the pipe after it is full, **number** will not be executed after **cat** is finished, this command will hang forever. This is not the correct implementation of a shell.

- **Please don't store data into temporary files.** You should only write to files when it is specified by file redirection ( > ). Although you might get the same results implementing this way, there are many drawbacks. Therefore, you will get **zero points** if we found out you secretly write data into temporary files when you shouldn't have.