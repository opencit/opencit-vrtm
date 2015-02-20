#!/bin/bash

source utils/functions

# The script does the following
# 1. Creates the directory structure ../dist, kvm_build
# 2. Copies all the resources to kvm_build
# 3. Builds libvirt, rpcore
# 4. Creates a tar for the final install
# 5. Copies the tar and install scripts to dist

START_DIR=$PWD
SRC_ROOT_DIR=$START_DIR/../
BUILD_DIR=$START_DIR/KVM_build
DIST_DIR=$PWD/../dist

function makeDirStructure()
{
	if [ -d $SRC_ROOT_DIR/rpcore -a -d $SRC_ROOT_DIR/rpclient -a -d $SRC_ROOT_DIR/docs -a -d $SRC_ROOT_DIR/install ] 
	then
		echo "All resources found"
	else
		echo "One or more of the required dirs absent please check the following dir existence :"
		echo "rpcore, rpclient, docs and install at $SRC_ROOT_DIR"
		exit
	fi
	mkdir -p $BUILD_DIR
	cp -r $SRC_ROOT_DIR/rpcore $SRC_ROOT_DIR/rpclient $SRC_ROOT_DIR/docs $SRC_ROOT_DIR/install $BUILD_DIR 
}

function buildLibvirt()
{

	if [ ! -f libvirt-1.2.2.tar.gz ]
	then
		echo "Downloading libvirt-1.2.2"
		wget http://libvirt.org/sources/libvirt-1.2.2.tar.gz
		if [ $? -ne 0 ] ; then
			echo "Libvirt download failed... Pl check outfile for errors"
			exit
		fi
	fi

	if [ ! -f libvirt-1.2.2/src/.libs/libvirt.so.0.1002.2 ]
	then
		tar xf libvirt-1.2.2.tar.gz
		cd libvirt-1.2.2
		echo Building libvirt - may take a few minutes
		./configure > $BUILD_DIR/outfile 2>&1
		make >> $BUILD_DIR/outfile 2>&1
		if [ $? -ne 0 ] ; then
			echo "Libvirt build failed ... Pl check outfile for errors"
			exit
		fi
		cd ..
	fi
 	cp -a libvirt-1.2.2/include/libvirt rpcore/src/rpchannel
	# This dir is not created in the structure
	mkdir -p rpcore/lib
	cp -pL libvirt-1.2.2/src/.libs/libvirt.so.0.1002.2 rpcore/lib/libvirt.so
	cd $BUILD_DIR
}


function buildVerifier()
{
	cd rpcore/src/imvm
	make -f verifier-g.mak clean >> $BUILD_DIR/outfile 2>&1
    if [ $? -ne 0 ]; then
        echo "Verifier clean failed...Please see outfile for more details"
        exit
	else
		echo "Verifier clean successful"
    fi
	make -f verifier-g.mak >> $BUILD_DIR/outfile 2>&1
    if [ $? -ne 0 ]; then
        echo "Verifier build failed...Please see outfile for more details"
        exit
    else
        echo "Verifier build successful"
    fi
	cd $BUILD_DIR
}

function buildRplistener()
{
	cd rpcore/src/rpproxy/kvm_proxy
    make -f rp-proxy.mak clean >> $BUILD_DIR/outfile 2>&1
	if [ $? -ne 0 ]; then
        echo "RP-Proxy clean failed...Please see outfile for more details"
        exit
    else
        echo "RPProxy clean successful"
	fi

        make -f rp-proxy.mak >> $BUILD_DIR/outfile 2>&1
	if [ $? -ne 0 ]; then
                echo "RP-Proxy build failed...Please see outfile for more details"
                exit
        else
                echo "RPProxy build successful"
        fi
	
        cd $BUILD_DIR
}

function buildRpcore()
{
    cd $BUILD_DIR/rpcore
	#mkdir -p bin/debug bin/release build/debug build/release lib
	cd src
    make clean >> $BUILD_DIR/outfile 2>&1
    if [ $? -ne 0 ]; then
        echo "RPcore clean failed...Please see outfile for more details"
        exit
    else
        echo "RPCore clean successful"
    fi
    make >> $BUILD_DIR/outfile 2>&1
	if [ $? -ne 0 ]; then
		echo "RPcore build failed...Please see outfile for more details"
		exit
	else
        echo "RPCore build successful"
	fi
    cd $BUILD_DIR
}

function install_kvm_packages()
{
	echo "Installing Required Packages ....."
	apt-get update
	apt-get --force-yes -y install gcc libsdl1.2-dev zlib1g-dev libasound2-dev linux-kernel-headers pkg-config libgnutls-dev libpci-dev build-essential bzr bzr-builddeb cdbs debhelper devscripts dh-make diffutils dpatch fakeroot gnome-pkg-tools gnupg liburi-perl lintian patch patchutils pbuilder piuparts quilt ubuntu-dev-tools wget libglib2.0-dev libsdl1.2-dev libjpeg-dev libvde-dev libvdeplug2-dev libbrlapi-dev libaio-dev libfdt-dev texi2html texinfo info2man pod2pdf libnss3-dev libcap-dev libattr1-dev libtspi-dev gcc-4.6-multilib libpixman-1-dev libxml2-dev libssl-dev ant
	apt-get --force-yes -y install libyajl-dev libdevmapper-dev libpciaccess-dev libnl-dev
	apt-get --force-yes -y install bridge-utils dnsmasq pm-utils ebtables ntp chkconfig
	apt-get --force-yes -y install openssh-server
	apt-get --force-yes -y install python-dev
	
	echo "Starting ntp service ....."
	service ntp start
	chkconfig ntp on

}

function main()
{
	echo "Installing the packages required for KVM... "	
	install_kvm_packages
	echo "Creating the required build structure... "
	makeDirStructure
	echo "Building RPCore binaries... "
        buildRpcore
	cd $BUILD_DIR
	echo "Building libvirt if required..."
	buildLibvirt
	echo "Building Verifier binaries..."
	buildVerifier
	echo "Building RPListener binaries..."
	buildRplistener

	BUILD_VER=`date +%Y%m%d%H%M%S`
	grep -i ' error' $BUILD_DIR/outfile
	
	echo "Verifying for any errors, please verify the output below : "
	arg=`cat $BUILD_DIR/outfile | grep -i -v "print*" | grep -c ' error'`

	echo "Create the install tar file"
        mkdir -p $DIST_DIR
        cd $BUILD_DIR
        tar czf KVM_install_$BUILD_VER.tar.gz libvirt-1.2.2.tar.gz rpcore/scripts rpcore/bin rpcore/rptmp rpcore/lib
        mv KVM_install_$BUILD_VER.tar.gz $DIST_DIR
        cp install/vRTM_KVM_install.sh $DIST_DIR
        
        arg=`cat $BUILD_DIR/outfile | grep -i -v "print*" | grep -c ' error'`

	if [ $arg -eq 0 ]
	then
	    echo "Build $KVM_install_$BUILD_VER.tar.gz created successfully !!"
	else
	    echo "Verifying for any errors, please verify the output below : "
	    echo Install tar file has been created but it might might has some errors. Please see $BUILD_DIR/outfile file.
	fi

}

function help_display()
{
    echo "Usage: ./vRTM_KVM_build.sh [Options]"
    echo ""
    echo "This script does the following"
    echo "1. Creates the directory structure ../dist, ./kvm_build"
    echo "2. Copies all the resources to kvm_build"
    echo "3. Builds vRTM components"
    echo "4. Creates a tar for the final install in ../dist folder"
    echo ""
    echo ""
    echo "Following options are available:"
    echo "--build"
    echo "--clean"
    echo "--help"
}

if [ $# -gt 1 ] 
then	
	echo "ERROR: Extra arguments"
	help_display
fi

if [ $# -eq 0 ] || [ "$1" == "--build" ]
then
	echo "Building vRTM"
	main
elif [ "$1" == "--clean" ]
then
	echo "Removing older build folder"
	rm -rf $BUILD_DIR
	rm -rf $DIST_DIR
else 
	help_display
fi