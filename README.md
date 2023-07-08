# cpp_shell
A shell like program that allows different users chat and apply commands. A homework from NYCU network programming class.

## Usage
1. ```cd 0812250/``` and use command ```make```
2. Open terminal and run **np_single_proc <port_number>** as server, ex: ```./np_single_proc 8888```
3. Open another terminal (or you can use tmux) and use telnet to connect, ex: ```telnet <ip_address> <port_number>```
4. Enjoy each feature mentioned below

## Features
### Basic
Simple features can be implemented by yourself, there are a few built-in functions inside already (written by TAs of this class)
Includes:
- **cat** : same as cat from shell
- **ls** : same as ls from shell
- **noop** : a program that do nothing 
- **number** : Add number to each line of input 
- **removetag** : remove HTML tags and output to STDOUT 
- **removetag0** : same as removetag but outputs error message to STDERR 
- **any program you add to bin/**

### Built-in commands
- **setenv** : set environment path
  - ```setenv PATH <path>```
- **printenv** : print environment path
  - ```printenv PATH```
- **who** : show information of all users
  - ```
    example:
    % who
    <ID> <nickname> <IP:port> <indicate me>
    1 IamStudent 140.113.215.62:1201 <-me
    2 (no name) 140.113.215.63:1013
    3 student3 140.113.215.62:1201
    ```
- **tell** : send message to another user
  - ``` % tell <user id> <message> ```
- **yell** : send message to all users
  - ``` % yell <message> ```
- **name** : change your name
  - ``` % name <new name> ```
- **exit** : to exit the shell

### Pipe
- Redirect input or output by using '<' ans '>'
- Allows **normal pipe** by using '|'
- Allows **number pipe** by using "|" + number
  - ex: "|2" will pipe the STDOUT to the next next command
  - ```
    % removetag test.html | number
    1
    2 Test
    3 This is a test program
    4 for ras.
    5
    
    % removetag test.html |2
    % ls
    bin test.html test1.txt test2.txt

    % number
    1
    2 Test
    3 This is a test program
    4 for ras.
    5
    ```
- Allows **user pipe** , allows user to pipe their STDOUT to another user, by using ">" + "userId"
  - ex: you are user1, then you can user ">2" to pipe your STDOUT to user2,
        and user2 will need "<1"
  - note that before the user you pipe to receive your piped message, you can't pipe other messages to him (but other users can)
  - ```
    user1:
    % cat test.html >2

    user2:
    % cat <1
    content from test.html
    ```
- Note that you **can not** make one input pipe to two different places
- You **can** get other user's pipe message and redirect it as normal message
  - ex: these commands are allowed
  - ```
    situation: you are user2 and user1 just piped some messages to you
    
    % cat <1 | number
    % cat <1 >3
    % cat <1 |2
    % cat <1 > test.txt
    ```

### Examples
```
User1:

bash$ telnet nplinux1.nctu.edu.tw 7001
***************************************
** Welcome to the information server **
*************************************** # Welcome message
*** User ’(no name)’ entered from 140.113.215.62:1201. *** # Broadcast message of user login

% who
<ID> <nickname> <IP:port> <indicate me>
1 (no name) 140.113.215.62:1201 <- me

% name Jamie
*** User from 140.113.215.62:1201 is named ’Jamie’. ***

% ls
bin test.html

% *** User ’(no name)’ entered from 140.113.215.63:1013. *** # User 2 logins
who
<ID> <nickname> <IP:port> <indicate me>
1 Jamie 140.113.215.62:1201 <- me
2 (no name) 140.113.215.63:1013

% *** User from 140.113.215.63:1013 is named ’Roger’. *** # User 2 inputs ’name Roger’
who
<ID> <nickname> <IP:port> <indicate me>
1 Jamie 140.113.215.62:1201 <- me
2 Roger 140.113.215.63:1013

% *** User ’(no name)’ entered from 140.113.215.64:1302. *** # User 3 logins
who
<ID> <nickname> <IP:port> <indicate me>
1 Jamie 140.113.215.62:1201 <- me
2 Roger 140.113.215.63:1013
3 (no name) 140.113.215.64:1302

% yell Who knows how to make egg fried rice? help me plz!
*** Jamie yelled ***: Who knows how to make egg fried rice? help me plz!

% *** (no name) yelled ***: Sorry, I don’t know. :-( # User 3 yells
*** Roger yelled ***: HAIYAAAAAAA !!! # User 2 yells

% tell 2 Plz help me, my friends!

% *** Roger told you ***: Yeah! Let me show you the recipe # User 2 tells to User 1
*** Roger (#2) just piped ’cat EggFriedRice.txt >1’ to Jamie (#1) *** # Broadcast message of user pipe
*** Roger told you ***: You can use ’cat <2’ to show it!
cat <5 # mistyping
*** Error: user #5 does not exist yet. ***

% cat <2 # receive from the user pipe
*** Jamie (#1) just received from Roger (#2) by ’cat <2’ ***
Ask Uncle Gordon
Season with MSG !!

% tell 2 It’s works! Great!

% *** Roger (#2) just piped ’number EggFriedRice.txt >1’ to Jamie (#1) ***
*** Roger told you ***: You can receive by your program! Try ’number <2’!
number <2
*** Jamie (#1) just received from Roger (#2) by ’number <2’ ***
1 1 Ask Uncle Gorgon
2 2 Season with MSG !!

% tell 2 Cool! You’re genius! Thank you!
2

% *** Roger told you ***: You’re welcome!
*** User ’Roger’ left. ***
exit
bash$
```

```
User2

bash$ telnet nplinux1.nctu.edu.tw 7001 # The server port number
***************************************
** Welcome to the information server **
***************************************
*** User ’(no name)’ entered from 140.113.215.63:1013. ***

% name Roger
*** User from 140.113.215.63:1013 is named ’Roger’. ***

% *** User ’(no name)’ entered from 140.113.215.64:1302. ***
who
<ID> <nickname> <IP:port> <indicate me>
1 Jamie 140.113.215.62:1201
2 Roger 140.113.215.63:1013 <- me
3 (no name) 140.113.215.64:1302

% *** Jamie yelled ***: Who knows how to make egg fried rice? help me plz!
*** (no name) yelled ***: Sorry, I don’t know. :-(
yell HAIYAAAAAAA !!!
*** Roger yelled ***: HAIYAAAAAAA !!!

% *** Jamie told you ***: Plz help me, my friends!
tell 1 Yeah! Let me show you the recipe

% cat EggFriedRice.txt >1 # write to the user pipe
*** Roger (#2) just piped ’cat EggFriedRice.txt >1’ to Jamie (#1) ***

% tell 1 You can use ’cat <2’ to show it!

% *** Jamie (#1) just received from Roger (#2) by ’cat <2’ ***
*** Jamie told you ***: It’s works! Great!
number EggFriedRice.txt >1
*** Roger (#2) just piped ’number EggFriedRice.txt >1’ to Jamie (#1) ***

% tell 1 You can receive by your program! Try ’number <2’!

% *** Jamie (#1) just received from Roger (#2) by ’number <2’ ***
*** Jamie told you ***: Cool! You’re genius! Thank you!
tell 1 You’re welcome!

% exit
bash$
```
