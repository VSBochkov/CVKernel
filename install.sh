sudo sh -c "echo '\nCVKERNEL_PATH=`pwd`\n' >> /etc/environment"
sudo ln -s `pwd`/Release/CVKernel /usr/local/bin/CVKernel
cd ./python
sudo python setup.py install
