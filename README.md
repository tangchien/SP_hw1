# SP_hw1

Valid id are from 902001 to 902020

### How to run the program (read_server)
```terminal=
  cd SP_hw1
  make
  ./read_server {portnum} &
  telnet 127.0.0.1 {portnum}
```

### How to run the program (write_server)
```terminal=
  cd SP_hw1
  make
  ./write_server {portnum} &
  telnet 127.0.0.1 {portnum}
```

#### What I do
I add the poll to solve the busy waiting issue, and I add the lock on the file to solve the race condition when there are multiple connection.


