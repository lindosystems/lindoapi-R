rLindo - The R interface to LINDO API
=====================================

Requirements
------------

1.  LINDO API x.y [(https://www.lindo.com)](https://www.lindo.com/index.php/ls-downloads/try-lindo-api), where x.y is the version tag, e.g. 15.0
    
2.  R software 3.5 or later [(https://cran.r-project.org/index.html)](https://cran.r-project.org/index.html).
    
3.  Rtools (Windows only) [(http://www.cran.r-project.org/doc/manuals/R-admin.html#The-Windows-toolset)](http://www.cran.r-project.org/doc/manuals/R-admin.html#The-Windows-toolset).
    

Installation for Windows
------------------------

**NOTE**: Make sure to run the command window as administrator to perform these steps.

1.  Make sure environment variable LINDOAPI\_HOME points to the installation path of LINDO API. If you used the standard installer this variable should already be set. Otherwise, you should set it manually.
    
        C:> set LINDOAPI_HOME="c:\lindoapi"
        
    
2.  Navigate to the directory `c:\lindoapi\R`, where the file `rLindo_x.y.tar.gz` is located.
    
3.  Run the following batch script to install the rLindo package, where `<nn>` stands for the rtools version. 
    
        C:> install-rlindo-r<nn>.bat
        
    For example, for R version R42, run this command
    
        C:> install-rlindo-r42.bat
        
    You can edit `install-rlindo-r42.bat` and edit to make it compatible for later versions.
    
4.  Test whether rLindo package has been installed successfully by using the following command in R.
    
        > library(rLindo)
        
5.  You can test the samples from command-line by running this command from `c:\lindoapi\R` folder.

        > RSCRIPT samples/ex_nlp.R

Installation for Linux and Mac/OSX
----------------------------------

**NOTE**: Make sure to login as the 'root' user or use 'sudo' to perform these steps.

1.  Set environment variable LINDOAPI\_HOME to the installation path of LINDO API. e.g.
    
        $ export LINDOAPI_HOME=/opt/lindoapi
        
    
    To have this variable set automatically, add the above line to your `~/.bashrc` file.
    
2.  Navigate to the directory `/opt/lindoapi/R`, where the file `rLindo_x.y.tar.gz` is located.
    
3.  Run the following batch script to install the rLindo package.
    
        $ ./install-rlindo.sh
        
    
    For Mac/OSX users, if you get an error message like below:
    
        checking for gcc... llvm-gcc-4.2 -arch x86_64 -std=gnu99
        checking whether the c compiler works...no
        
    
    link llvm-gcc-4.2 to your gcc compiler using command:
    
        $ sudo ln -fs llvm-gcc llvm-gcc-4.2
        
    
4.  For Mac/OSX users, you may need to change the install name of `rLindo.so` using this command, 
    
        $ RLIBRARY_NAME=site-library
        
        $ install_name_tool -change @loader_path/liblindo64.x.y.dylib \
                             ${LINDOAPI_HOME}/bin/osx64x86/liblindo64.x.y.dylib \
                             ${R_HOME}/$RLIBRARY_NAME/rLindo/libs/rLindo.so
        
    
    **Note**: The above command assumes the system is 64-bit and R\_HOME environment variable is defined. Otherwise, please revise the command accordingly.
    
5.  Test whether rLindo package has been installed successfully by using the following command in R.
    
        > library(rLindo)
        

6.  You can test the samples from command-line by running this command from `/opt/lindoapi/R` folder.

        $ Rscript samples/ex_nlp.R
        