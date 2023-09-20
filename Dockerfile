FROM ubuntu:jammy

RUN apt update
RUN apt install -y g++-12 gcc-12 build-essential cmake libssl-dev pkg-config git wget ninja-build nano libzip-dev

RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-12 60
RUN update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-12 60
RUN update-alternatives --install /usr/bin/cpp cpp /usr/bin/cpp-12 60

WORKDIR /opt

#Build a new enough boost, apt only contains 1.74 which is too old.
RUN wget https://boostorg.jfrog.io/artifactory/main/release/1.81.0/source/boost_1_81_0.tar.bz2
RUN tar xf boost_1_81_0.tar.bz2

WORKDIR /opt/boost_1_81_0

RUN ./bootstrap.sh --prefix=/usr
RUN ./b2 variant=release link=static threading=multi
RUN ./b2 variant=release link=static threading=multi install

WORKDIR /opt

#Build latest hiredis containing sdevent support, not available yet in apt
RUN wget https://github.com/redis/hiredis/archive/refs/tags/v1.2.0.tar.gz
RUN tar xf v1.2.0.tar.gz
RUN mkdir /opt/hiredis-1.2.0/build

WORKDIR /opt/hiredis-1.2.0/build
RUN cmake -GNinja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DDISABLE_TESTS=1 -DCMAKE_INSTALL_PREFIX=/usr -DENABLE_SSL=1 ..
RUN ninja && ninja install

RUN mkdir -p /opt/ichor/build

WORKDIR /opt/ichor/build

ENTRYPOINT ["/bin/bash", "-c"]

CMD ["cd /opt/ichor/build && cmake -GNinja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DICHOR_USE_SANITIZERS=0 -DICHOR_USE_HIREDIS=1 -DICHOR_USE_BOOST_BEAST=ON -DICHOR_USE_SPDLOG=ON /opt/ichor/src && ninja"]
