FROM acap-runtime:armv7hf-debug
RUN apt install -y ssh sshpass gdb-multiarch jq
WORKDIR /tmp
RUN wget https://github.com/fullstorydev/grpcurl/releases/download/v1.8.6/grpcurl_1.8.6_linux_x86_64.tar.gz &&\
    tar xfz grpcurl_1.8.6_linux_x86_64.tar.gz &&\
    cp ./grpcurl /opt/app
WORKDIR /opt/app_host
CMD /bin/bash
