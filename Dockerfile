FROM archlinux:latest
RUN echo 'Server = http://ftp.jaist.ac.jp/pub/Linux/ArchLinux/$repo/os/$arch' > /etc/pacman.d/mirrorlist
RUN pacman -Syyu --noconfirm
RUN pacman -S git archiso arch-install-scripts --noconfirm
RUN git clone https://github.com/SereneTeam/alterlinux.git alterlinux/
WORKDIR /alterlinux
RUN git checkout dev-stable
RUN ./keyring.sh -ca
CMD ["./build.sh", "-b"]
