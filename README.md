# MOC
MPI Open MP Coordination library

# Configuration

To configure the MOC library, please run the following:

    $ ./autogen.sh && \
      ./configure \
          --prefix=$MOC_INSTALL_DIR \
          --with-mpi=$OMPI_INSTALL_DIR \
          --with-pmix=$PMIX_INSTALL_DIR \
      && \
      make && \
      make install

