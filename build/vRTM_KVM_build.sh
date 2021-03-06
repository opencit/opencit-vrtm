#!/bin/bash


# The script does the following
# 1. Creates the directory structure ../dist, kvm_build
# 2. Copies all the resources to kvm_build
# 3. Creates a tar for the final install
# 4. Copies the tar and install scripts to dist

START_DIR=$PWD
SRC_ROOT_DIR=$START_DIR/../
BUILD_DIR=$START_DIR/KVM_build
DIST_DIR=$PWD/../dist
TBOOT_REPO=${TBOOT_REPO:-"$SRC_ROOT_DIR/../dcg_security-tboot-xm"}

PACKAGE="vrtm/scripts vrtm/bin vrtm/lib vrtm/configuration"

# This function returns either rhel fedora ubuntu suse
# TODO : This function can be moved out to some common file
function getFlavour()
{
        flavour=""
        grep -c -i ubuntu /etc/*-release > /dev/null
        if [ $? -eq 0 ] ; then
                flavour="ubuntu" 
        fi
        grep -c -i "red hat" /etc/*-release > /dev/null
        if [ $? -eq 0 ] ; then
                flavour="rhel"
        fi
        grep -c -i fedora /etc/*-release > /dev/null
        if [ $? -eq 0 ] ; then 
                flavour="fedora"
        fi
	grep -c -i suse /etc/*-release > /dev/null
	if [ $? -eq 0 ] ; then
		flavour="suse"
	fi 
        if [ "$flavour" == "" ] ; then  
                echo "Unsupported linux flavor, Supported versions are ubuntu, rhel, fedora"
                exit -1
        else
                echo $flavour
        fi
}

function makeDirStructure()
{
	if [ -d "$SRC_ROOT_DIR/vrtm" -a -d "$SRC_ROOT_DIR/install" ]  #-a -d "$TBOOT_REPO/imvm" ] 
	then
		echo "All resources found"
	else
		echo "One or more of the required dirs absent please check the following dir existence :"
		echo "1. vrtm, rpclient, blueprints and install at $SRC_ROOT_DIR"
		#echo "2. Measurement Agent (imvm) is present at $TBOOT_REPO/imvm"
		exit -1
	fi
	mkdir -p "$BUILD_DIR"
	cp -r "$SRC_ROOT_DIR/vrtm" "$SRC_ROOT_DIR/install" "$BUILD_DIR"
}

# check wether log4cpp is installed on machine or not
function is_log4cpp_installed() {
	if [ -d /usr/include/log4cpp ]
	then
		echo "log4cpp devel package installed..."
	else
		echo "log4cpp devel package not installed..."
		echo "build failed..."
		exit -1;
	fi
		
	if [ -e /usr/lib/liblog4cpp.so -o -e /usr/lib64/liblog4cpp.so ]
	then
		echo "log4cpp libraries are present..."
	else
		echo "log4cpp libraries are not present..."
		echo "build failed..."
		exit -1;
	fi	
}

function buildvrtmproxy()
{
	cd vrtm/src/vrtmproxy/kvm_proxy
   	make -f vrtm-proxy.mak clean >> "$BUILD_DIR/outfile" 2>&1
	if [ $? -ne 0 ]; then
        echo "VRTM-Proxy clean failed...Please see outfile for more details"
        exit -1
    else
        echo "VRTMProxy clean successful"
	fi

        make -f vrtm-proxy.mak >> "$BUILD_DIR/outfile" 2>&1
	if [ $? -ne 0 ]; then
                echo "VRTM-Proxy build failed...Please see outfile for more details"
                exit -1
        else
                echo "VRTM-Proxy build successful"
        fi
	cd "$BUILD_DIR"
}

function buildvrtmlistener()
{
        cd vrtm/src/vrtmlistener/kvm_listener
        make -f vrtm-listener.mak clean >> "$BUILD_DIR/outfile" 2>&1
        if [ $? -ne 0 ]; then
        	echo "VRTM-listener clean failed...Please see outfile for more details"
	        exit -1
	else
        	echo "VRTM-listener clean successful"
        fi

        make -f vrtm-listener.mak >> "$BUILD_DIR/outfile" 2>&1
        if [ $? -ne 0 ]; then
                echo "VRTM-listener build failed...Please see outfile for more details"
                exit -1
        else
                echo "VRTM-listener build successful"
        fi
        cd "$BUILD_DIR"
}

function buildvrtmcore()
{
    cd "$BUILD_DIR/vrtm"
	#mkdir -p bin/debug bin/release build/debug build/release lib
	cd src

    echo > "$BUILD_DIR/outfile"
    make clean >> "$BUILD_DIR/outfile" 2>&1
    if [ $? -ne 0 ]; then
        echo "VRTMcore clean failed...Please see outfile for more details"
        exit -1
    else
        echo "VRTMCore clean successful"
    fi

    PYTHON_HEADERS=""
    if [ -e /usr/include/python2.7 ]
    then
        PYTHON_HEADERS="PY=/usr/include/python2.7 PYTHON=python2.7"
    elif [ -e /usr/include/python2.6 ]
    then
        PYTHON_HEADERS="PY=/usr/include/python2.6 PYTHON=python2.6"
    fi
    make $PYTHON_HEADERS >> "$BUILD_DIR/outfile" 2>&1
	if [ $? -ne 0 ]; then
		echo "VRTMcore build failed...Please see outfile for more details"
		exit -1
	else
	        echo "VRTMCore build successful"
	fi
    cd "$BUILD_DIR"
}


function install_kvm_packages_rhel()
{
	echo "Enabling epel-testing repo for log4cpp"
	sudo -n yum-config-manager --enable epel-testing > /dev/null
	if [ $? -ne 0 ]
	then
		echo "can't enable the epel-testing repo"
		echo "log4cpp might not get installed on RHEL-7"
	else
		echo "enabled epel-testing repo"
	fi
	echo "Installing Required Packages ....."
	sudo -n yum -y groupinstall -y "Development Tools" "Development Libraries"
	PackageList1=`echo $?`
	sudo -n yum install -y "kernel-devel-uname-r == $(uname -r)" libvirt-devel libxml2 gcc-c++ gcc make libxml2-devel openssl-devel
        PackageList2=`echo $?`
        if [ $PackageList1 -ne 0 ] || [ $PackageList2 -ne 0 ]; then
                echo "Failed to install pre-requisite packages"
                exit -1
 	else
		echo "Pre-requisite packages installed successfully"
	fi
}

function install_kvm_packages_ubuntu()
{
	echo "Installing Required Packages for ubuntu ....."
	sudo -n apt-get install -y gcc build-essential make libxml2-dev libssl-dev libvirt-dev --force-yes
	if [ $? -ne 0 ]; then
                echo "Failed to install pre-requisite packages"
                exit -1
        else
                echo "Pre-requisite packages installed successfully"

	fi
}

function install_kvm_packages_suse()
{
	echo "Installing Required Packages for sue ....."
	sudo -n zypper -n in make gcc gcc-c++ libxml2-devel libopenssl-devel pkg-config libgnutls-devel bzr debhelper devscripts dh-make diffutils perl-URI  wget glib2-devel libvirt-devel
	if [ $? -ne 0 ]; then
                echo "Failed to install pre-requisite packages"
                exit -1
        else
                echo "Pre-requisite packages installed successfully"
	fi
}

function install_kvm_packages()
{
	if [ $FLAVOUR == "ubuntu" ] ; then
		install_kvm_packages_ubuntu
	elif [  $FLAVOUR == "rhel" -o $FLAVOUR == "fedora" ] ; then
		install_kvm_packages_rhel
	elif [ $FLAVOUR == "suse" ] ; then
		install_kvm_packages_suse	
	fi
}

function makeUnixExecutable()
{
	dirName=$1
	tempFile=$RANDOM
	for file in `find "$dirName" -name "*.sh"`
	do
		# convert from dos2unix
		tr -d '\r' < "$file" > /tmp/tempFile
		mv /tmp/tempFile "$file"
		# Apply executable permissions
		chmod +x $file
	done
}

function log4cpp_inst_ubuntu()
{
        echo "Installing log4cpp devel for Ubuntu..."
        sudo -n apt-get install -y liblog4cpp5-dev
        if [ `echo $?` -ne 0 ]
        then
                echo "Failed to install log4cpp devel..."
                exit -1
        fi
        echo "Successfully installed log4cpp"
}

function log4cpp_inst_fedora()
{
        echo "Installing log4cpp devel for Fedora..."
        sudo -n yum install -y log4cpp-devel.x86_64
        if [ `echo $?` -ne 0 ]
        then
                echo "Failed to install log4cpp devel..."
                exit -1
        fi
        echo "Successfully installed log4cpp"
}

function log4cpp_inst_redhat()
{
        echo "Installing log4cpp devel for Redhat..."
        cd /tmp
        #wget ftp://195.220.108.108/linux/centos/6.6/os/x86_64/Packages/log4cpp-1.0-13.el6_5.1.x86_64.rpm
        wget http://ftp.redhat.com/pub/redhat/linux/enterprise/6Server/en/os/SRPMS/log4cpp-1.0-13.el6_5.1.src.rpm
        if [ `echo $?` -eq 0 ]
        then
                echo "log4cpp devel is successfully downloaded..."
        else
                echo "failed to download src rpm"
		cd "$BUILD_DIR"
                exit -1
        fi
        rpmbuild --rebuild /tmp/log4cpp-1.0-13.el6_5.1.src.rpm
        rpm -ivh ~/rpmbuild/RPMS/x86_64/log4cpp-1.0-13.el6.1.x86_64.rpm
        #wget ftp://195.220.108.108/linux/centos/6.6/os/x86_64/Packages/log4cpp-devel-1.0-13.el6_5.1.x86_64.rpm
        rpm -ivh ~/rpmbuild/RPMS/x86_64/log4cpp-devel-1.0-13.el6.1.x86_64.rpm
	cd "$BUILD_DIR"
}

function log4cpp_inst_suse()
{
        echo "Installing log4cpp devel for Suse..."
        cd /tmp
        wget ftp://195.220.108.108/linux/centos/6.6/os/x86_64/Packages/log4cpp-devel-1.0-13.el6_5.1.x86_64.rpm
        sudo -n zypper -n install log4cpp-devel-1.0-13.el6_5.1.x86_64.rpm
        if [ `echo $?` -eq 0 ]
        then
                echo "log4cpp devel is successfully installed..."
                cp -Pv /usr/lib64/liblog4cpp* /usr/local/lib/
                cd $install_dir
        else
                echo "Failed to download log4cpp library..."
                cd $install_dir
                exit -1
        fi
}

function install_log4cpp()
{
	if [ $FLAVOUR == "ubuntu" ] ; then
		log4cpp_inst_ubuntu
	elif [ $FLAVOUR == "rhel" ] ; then
		log4cpp_inst_redhat	
	elif [ $FLAVOUR == "fedora" ] ; then
                log4cpp_inst_fedora
        elif [ $FLAVOUR == "suse" ] ; then
                log4cpp_inst_suse
	fi
}

function main()
{
	FLAVOUR=`getFlavour`
	echo "Installing the packages required for KVM... "	
	install_kvm_packages

        echo "Creating the required build structure... "
        makeDirStructure

	echo "Installing log4cpp devel..."
	install_log4cpp

	#TODO:check if log4cpp-devel is installed on the machine 
	echo "Checking log4cpp-devel is installed on machine..."
	is_log4cpp_installed
	echo "Building VRTMCore binaries... "
        buildvrtmcore
	cd "$BUILD_DIR"
	
        # echo "Building VRTMListener binaries..."
	# buildvrtmlistener

        echo "Building VRTMProxy binaries..."
	buildvrtmproxy

	grep -i ' error' "$BUILD_DIR/outfile"
	
	echo "Verifying for any errors, please verify the output below : "
	arg=`cat "$BUILD_DIR/outfile" | grep -i -v "print*" | grep -c ' error'`

	echo "Create the install tar file"
        mkdir -p "$DIST_DIR"
        cd "$BUILD_DIR"
	makeUnixExecutable $BUILD_DIR
        tar czf KVM_install.tar.gz $PACKAGE
        mv KVM_install.tar.gz "$DIST_DIR"
        cp install/vRTM_KVM_install.sh "$DIST_DIR"
	tr -d '\r' < "$DIST_DIR/vRTM_KVM_install.sh" > /tmp/output.file
	mv /tmp/output.file "$DIST_DIR/vRTM_KVM_install.sh"
	chmod +x "$DIST_DIR/vRTM_KVM_install.sh"
        arg=`cat "$BUILD_DIR/outfile" | grep -i -v "print*" | grep -c ' error'`

	if [ $arg -eq 0 ]
	then
	    echo "Build KVM_install.tar.gz created successfully !!"
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
	rm -rf "$BUILD_DIR"
	rm -rf "$DIST_DIR"
else 
	help_display
fi
