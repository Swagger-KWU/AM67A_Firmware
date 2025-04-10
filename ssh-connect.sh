#!/bin/sh
ssh-keygen -f "/home/juwon/.ssh/known_hosts" -R "192.168.7.2"
sshfs -o StrictHostKeyChecking=no root@192.168.7.2:/ /home/juwon/SSH-DIR/
ssh root@192.168.7.2
