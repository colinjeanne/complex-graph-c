# Get the base Ubuntu image from Docker Hub
FROM ubuntu:latest

# Update apps on the base image
RUN apt-get -y update && apt-get install -y && apt-get clean && \
  apt-get -y install clang cmake git-all lldb llvm libpng-dev
