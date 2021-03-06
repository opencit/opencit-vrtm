#!/bin/bash

#This script installs vrtmCore, vrtmProxy, vrtmListener, and openstack patches


RES_DIR=$PWD
DEFAULT_INSTALL_DIR=/opt

OPENSTACK_DIR="Openstack/patch"
DIST_LOCATION=`/usr/bin/python -c "from distutils.sysconfig import get_python_lib; print(get_python_lib())"`

LINUX_FLAVOUR="ubuntu"
NON_TPM="false"
BUILD_LIBVIRT="FALSE"
KVM_BINARY=""
LOG_DIR="/var/log/vrtm"
VERSION_INFO_FILE=vrtm.version

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
        if [ $? -eq 0 ] && [ $flavour == "" ] ; then
                flavour="fedora"
        fi
        grep -c -i suse /etc/*-release > /dev/null
        if [ $? -eq 0 ] ; then
                flavour="suse"
        fi
        if [ "$flavour" == "" ] ; then
                echo "Unsupported linux flavor, Supported versions are ubuntu, rhel, fedora"
                exit
        else
                echo $flavour
        fi
}

function updateFlavourVariables()
{
        linuxFlavour=`getFlavour`
        if [ $linuxFlavour == "fedora" ]
        then
             export KVM_BINARY="/usr/bin/qemu-kvm"
	     export QEMU_INSTALL_LOCATION="/usr/bin/qemu-system-x86_64"
	elif [ $linuxFlavour == "rhel" ]
	then
	     export KVM_BINARY="/usr/bin/qemu-kvm"
	     if [ -x /usr/bin/qemu-system-x86_64 ] ; then
		export QEMU_INSTALL_LOCATION="/usr/bin/qemu-system-x86_64"
	     else
	        export QEMU_INSTALL_LOCATION="/usr/libexec/qemu-kvm"
	     fi
	elif [ $linuxFlavour == "ubuntu" ]
	then
              export KVM_BINARY="/usr/bin/kvm"
	      export QEMU_INSTALL_LOCATION="/usr/bin/qemu-system-x86_64"
	elif [ $linuxFlavour == "suse" ]
	then
              export KVM_BINARY="/usr/bin/kvm"
              export QEMU_INSTALL_LOCATION="/usr/bin/qemu-system-x86_64"
        fi
}


function untarResources()
{
	cd "$RES_DIR"
	cp KVM_install.tar.gz "$INSTALL_DIR"
	cd "$INSTALL_DIR"
	tar xvzf KVM_install.tar.gz
        if [ $? -ne 0 ]; then
                echo "ERROR : Untarring of $RES_DIR/*.tar.gz unsuccessful"
                exit
        fi
	chmod 755 "$INSTALL_DIR/vrtm" "$INSTALL_DIR/vrtm/bin" "$INSTALL_DIR/vrtm/configuration" "$INSTALL_DIR/vrtm/lib" "$INSTALL_DIR/vrtm/scripts"
	chmod 755 "$INSTALL_DIR"/vrtm/lib/*
	chmod 755 "$INSTALL_DIR"/vrtm/scripts/mount_vm_image.sh
	chmod 766 "$INSTALL_DIR"/vrtm/configuration/vrtm_proxylog.properties
	rm -rf KVM_install.tar.gz
}

function installKVMPackages_rhel()
{
        echo "Enabling epel-testing repo for log4cpp"
        yum-config-manager --enable epel-testing > /dev/null
        if [ $? -ne 0 ]
        then
                echo "can't enable the epel-testing repo"
                echo "log4cpp might not get installed on RHEL-7"
        else
                echo "enabled epel-testing repo"
        fi
        echo "Installing Required Packages ....."
	if [ $DEPLOYMENT_TYPE == "vm" ]
	then
		#install guestmount only in VM mode
		yum install -y libguestfs-tools-c  kpartx lvm2
		if [ $? -ne 0 ]
		then
			echo "Failed to install guestfs-tools and mounting tools"
			exit -1
		fi
	fi
        yum install -y tar procps binutils
	if [ $? -ne 0 ]; then
                echo "Failed to install pre-requisite packages"
                exit -1
        else
                echo "Pre-requisite packages installed successfully"
	fi
	selinuxenabled
	if [ $? -eq 0 ] ; then
		yum install -y policycoreutils-python
		if [ $? -ne 0 ]; then
                	echo "Failed to install pre-requisite packages"
                	exit -1
        	else
                	echo "Pre-requisite packages installed successfully"
		fi
	fi
}

function installKVMPackages_ubuntu()
{
	echo "Installing Required Packages ....."
	if [ "$DEPLOYMENT_TYPE" == "vm" ]
	then
		apt-get -y install libguestfs-tools qemu-utils kpartx lvm2
	else
		return
	fi
	if [ $? -ne 0 ]; then
                echo "Failed to install pre-requisite packages"
                exit -1
        else
                echo "Pre-requisite packages installed successfully"
        fi
}

function installKVMPackages_suse()
{
	if [ $DEPLOYMENT_TYPE == "vm" ]
	then
		zypper -n in libguestfs-tools-c kpartx lvm2
		if [ $? -ne 0 ]
		then
			echo "Failed to install guestfs-tools"
			exit -1
		fi
	fi
	zypper -n in wget
	if [ $? -ne 0 ]; then
                echo "Failed to install pre-requisite packages"
                exit -1
        else
                echo "Pre-requisite packages installed successfully"
        fi
}

function installKVMPackages()
{
        if [ $FLAVOUR == "ubuntu" ] ; then
		installKVMPackages_ubuntu
        elif [  $FLAVOUR == "rhel" -o $FLAVOUR == "fedora" ] ; then
		installKVMPackages_rhel
	elif [ $FLAVOUR == "suse" ] ; then
		installKVMPackages_suse
        fi
}

function installvrtmProxy()
{
	echo "Installing vrtmProxy...."

	if [ -e $KVM_BINARY ] ; then
		echo "#! /bin/sh" > $KVM_BINARY
		echo "exec $QEMU_INSTALL_LOCATION -enable-kvm \"\$@\""  >> $KVM_BINARY
		chmod +x $KVM_BINARY
	fi
	if [ -e /usr/bin/qemu-system-x86_64_orig ]
	then	
		echo "vrtm-Proxy binary is already updated, might be old and will be replaced" 
	else
		echo "Backup of /usr/bin/qemu-system-x86_64 taken"
		cp --preserve=all "$QEMU_INSTALL_LOCATION" /usr/bin/qemu-system-x86_64_orig
	fi
	cp "$INSTALL_DIR/vrtm/bin/vrtm_proxy" "$QEMU_INSTALL_LOCATION"
	
	#Verify rp-proxy replacement
	diff "$INSTALL_DIR/vrtm/bin/vrtm_proxy" "$QEMU_INSTALL_LOCATION" > /dev/null
	if [ $? -eq 0 ] ; then
		echo "vrtm-Proxy replaced successfully"
	else
		echo "ERROR : Could not replace vrtm_proxy with $QEMU_INSTALL_LOCATION"
		echo "Please execute following after ensuring VMs are shut-down and $QEMU_INSTALL_LOCATION is not is use"
		echo "\$ cp $INSTALL_DIR/vrtm/bin/vrtm_proxy $QEMU_INSTALL_LOCATION"
	fi

	chmod +x "$QEMU_INSTALL_LOCATION"
	
        if [ $FLAVOUR == "ubuntu" ]; then
		LIBVIRT_QEMU_FILE="/etc/apparmor.d/abstractions/libvirt-qemu"           
                if [ -e $LIBVIRT_QEMU_FILE ] ; then
		        vrtm_comment="#Intel CIT vrtm"
                        vrtm_end_comment="#End Intel CIT vrtm"
                        grep "$vrtm_comment" $LIBVIRT_QEMU_FILE > /dev/null
                        if [ $? -eq 1 ] ; then
                            echo "$vrtm_comment" >> $LIBVIRT_QEMU_FILE
                            echo "$INSTALL_DIR/vrtm/lib/libvrtmchannel.so rmix," >> $LIBVIRT_QEMU_FILE
                            echo "$INSTALL_DIR/vrtm/configuration/vrtm_proxylog.properties r," >> $LIBVIRT_QEMU_FILE
                            echo "$INSTALL_DIR/vrtm/configuration/vRTM.cfg r," >> $LIBVIRT_QEMU_FILE
                            echo "$LOG_DIR/vrtm_proxy.log w," >> $LIBVIRT_QEMU_FILE
                            echo "/usr/bin/qemu-system-x86_64_orig rmix," >> $LIBVIRT_QEMU_FILE
                            echo "$vrtm_end_comment" >> $LIBVIRT_QEMU_FILE	
                        fi
                        echo "Appended libvirt apparmour policy"
                elif [ -e /etc/apparmor.d/disable/usr.sbin.libvirtd ] ; then
                        echo "libvirt apparmor already disabled"
                else
                # Disable the apparmor profile for libvirt for ubuntu
                        ln -s /etc/apparmor.d/usr.sbin.libvirtd /etc/apparmor.d/disable/
                        apparmor_parser -R /etc/apparmor.d/usr.sbin.libvirtd 
			echo "Disabling apparmour policy for libvirtd"
                fi
        fi

	if [ $FLAVOUR == "rhel" -o $FLAVOUR == "fedora" ]; then
		if [ $FLAVOUR == "rhel" ] ; then
			rhel7_version=""
			rhel7_version=`cat /etc/redhat-release | grep -o "7\.."`
			if [ ! -z "$rhel7_version" ]
			then
				SELINUX_TYPE="svirt_tcg_t"
			else
				SELINUX_TYPE="svirt_t"
			fi
		else
			SELINUX_TYPE="svirt_tcg_t"
		fi
		selinuxenabled
		if [ $? -eq 0 ] ; then
			echo "Updating the selinux policies for vRTM files"
			 semanage fcontext -a -t virt_log_t $LOG_DIR
			 restorecon -v $LOG_DIR
			 semanage fcontext -a -t virt_etc_t $INSTALL_DIR/vrtm/configuration/vrtm_proxylog.properties
		         restorecon -v $INSTALL_DIR/vrtm/configuration/vrtm_proxylog.properties
			 semanage fcontext -a -t virt_etc_t $INSTALL_DIR/vrtm/configuration/vRTM.cfg
		         restorecon -v $INSTALL_DIR/vrtm/configuration/vRTM.cfg
			 semanage fcontext -a -t qemu_exec_t "$QEMU_INSTALL_LOCATION"
			 restorecon -v "$QEMU_INSTALL_LOCATION"
			 semanage fcontext -a -t qemu_exec_t $INSTALL_DIR/vrtm/lib/libvrtmchannel.so
			 restorecon -v $INSTALL_DIR/vrtm/lib/libvrtmchannel.so  
                         echo " 
                               module svirt_for_links 1.0;
                                
                               require {
                               type nova_var_lib_t;
                               type $SELINUX_TYPE;
			 " > svirt_for_links.te
			 if [ ! -z "$rhel7_version" ]
			 then
			 
			 echo "
			       type var_log_t;
			 " >> svirt_for_links.te
			 fi
			 echo "
                               class lnk_file read;
			 " >> svirt_for_links.te
			 if [ ! -z "$rhel7_version" ]
			 then
			 
			 echo "
			       class file read;
			       class file write;
			       class file open;
			 " >> svirt_for_links.te
			 fi 
			 echo "
                               }
                               #============= svirt_t ==============
                               allow $SELINUX_TYPE nova_var_lib_t:lnk_file read;
			  " >> svirt_for_links.te
			  if [ ! -z "$rhel7_version" ]
			  then
			  echo "
			       allow svirt_tcg_t var_log_t:file { read write open };
                          " >> svirt_for_links.te
			  fi
                          /usr/bin/checkmodule -M -m -o svirt_for_links.mod svirt_for_links.te
                          /usr/bin/semodule_package -o svirt_for_links.pp -m svirt_for_links.mod
                          /usr/sbin/semodule -i svirt_for_links.pp
						  rm -rf svirt_for_links.mod svirt_for_links.pp svirt_for_links.te
		else
			echo "WARN : Selinux is disabled, enabling SELinux later will conflict vRTM"
		fi
	fi
	ldconfig
	cd "$INSTALL_DIR"
}

function startNonTPMRpCore()
{
    /usr/local/bin/vrtm stop
    sleep 5
	echo "Starting non-TPM vrtmCORE...."
	/usr/local/bin/vrtm start
}

function createvRTMStartScript()
{

	if [ $FLAVOUR = "ubuntu" ] ;then 
		export	LIBVIRT_SERVICE_NAME="libvirt-bin"
	elif [ $FLAVOUR = "rhel" -o $FLAVOUR = "fedora" -o $FLAVOUR = "suse" ] ;then
		export LIBVIRT_SERVICE_NAME="libvirtd"
	fi

	VRTM_SCRIPT="$INSTALL_DIR/vrtm/scripts/vrtm.sh"
	echo "Creating the startup script.... $VRTM_SCRIPT"
	touch $VRTM_SCRIPT 
	echo "#!/bin/bash

### BEGIN INIT INFO
# Provides:          vrtm
# Required-Start:    \$local_fs \$network \$remote_fs
# Required-Stop:     \$local_fs \$network \$remote_fs
# Default-Start:     2 3 4 5
# Default-Stop:      0 1 6
# Should-Start:    $LIBVIRT_SERVICE_NAME
# Should-Stop:	     $LIBVIRT_SERVICE_NAME
# Short-Description: VRTM
# Description:       Virtual Root Trust Management
### END INIT INFO
	tagent_availability()
	{
		tagent_bin=\"\"
		tagent_bin=\"\`which tagent\`\"
		if [ -z \"\$tagent_bin\" ]
		then
			export PATH=\"\$PATH:/usr/local/bin\"
			tagent_bin=\"\`which tagent\`\"
			if [ -z \"\$tagent_bin\" ]
			then
				return 1
			else
				return 0
			fi
		fi
		return 0
	}
	startVrtm()
	{
		#chown -R nova:nova /var/run/libvirt/
		if ! tagent_availability
		then
			echo \"tagent not found\"
			echo \"can't start vrtm\"
			return 1
		fi
		ldconfig
        	cd \"$INSTALL_DIR/vrtm/bin\"
        	nohup ./vrtmcore > /var/log/vrtm/vrtm_crash.log 2>&1 &
	}
	
	case \"\$1\" in
	 start)
	    pgrep vrtmcore
	    if [ \$? -ne 0 ] ; then
	        echo \"Starting vrtm...\"
	        startVrtm
	    else
	        echo \"VRTM is already running...\"
	    fi  
	   ;;
	 stop)
	        echo \"Stopping all vrtm processes (if any ) ...\"
	        pkill vrtmcore
	   ;;
	 version)
		cat \"$INSTALL_DIR/vrtm/$VERSION_INFO_FILE\"
	   ;;
	 *)
	   echo \"Usage: {start|stop|version}\" >&2
	   exit 3
	   ;;
	esac
	" > "$VRTM_SCRIPT"
	chmod +x "$VRTM_SCRIPT"
	rm -rf /usr/local/bin/vrtm
	ln -s "$VRTM_SCRIPT" /usr/local/bin/vrtm
}

function validate()
{
	# Validate the following 
	# qemu-kvm is libvirt 1.2.2 is installed
	# nova-compute is installed

	# Validate qemu-kmv installation	
	if [ ! -e $QEMU_INSTALL_LOCATION ] ; then
		echo "ERROR : Could not find $QEMU_INSTALL_LOCATION installed on this machine"
		echo "Please install qemu kvm"
		exit
	fi
}

function log4cpp_inst_ubuntu()
{
        is_ubuntu_16=`lsb_release -a | grep "^Release" | grep 16.04`
        if [ -n "$is_ubuntu_16" ]
        then
            echo "Installing log4cpp for Ubuntu 16.04... "
            apt-get -y install liblog4cpp5v5
        else
            echo "Installing log4cpp for Ubuntu other than 16.04..."
            apt-get -y install liblog4cpp5
        fi

        if [ `echo $?` -ne 0 ]
        then
                echo "Failed to install log4cpp..."
                exit -1
        fi
        echo "Successfully installed log4cpp"
}

function log4cpp_inst_fedora()
{
        echo "Installing log4cpp for Fedora..."
        yum install -y log4cpp.x86_64
        if [ `echo $?` -ne 0 ]
        then
                echo "Failed to install log4cpp..."
                exit -1
        fi
        echo "Successfully installed log4cpp"
}

function log4cpp_inst_redhat()
{
        echo "Installing log4cpp for Redhat..."
         yum install -y log4cpp.x86_64
         if [ `echo $?` -ne 0 ]
         then
                 echo "Failed to install log4cpp..."
                 exit -1
         fi
         echo "Successfully installed log4cpp"
}

function log4cpp_inst_suse()
{
        echo "Installing log4cpp for Suse..."
        cd /tmp
        wget ftp://195.220.108.108/linux/centos/6.6/os/x86_64/Packages/log4cpp-1.0-13.el6_5.1.x86_64.rpm
        zypper -n install log4cpp-1.0-13.el6_5.1.x86_64.rpm
        if [ `echo $?` -eq 0 ]
        then
                cp -Pv /usr/lib64/liblog4cpp* /usr/local/lib/
                cd $install_dir
        else
                echo "Failed to install log4cpp..."
                cd $install_dir
                exit -1
        fi
        echo "Successfully installed log4cpp"
	cd "$BUILD_DIR"
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

function main_default()
{
  if [ -z "$INSTALL_DIR" ]; then
    INSTALL_DIR="$DEFAULT_INSTALL_DIR"
  fi
  mkdir -p "$INSTALL_DIR"
  
	FLAVOUR=`getFlavour`
	updateFlavourVariables
        cd "$INSTALL_DIR"

        echo "Installing pre-requisites ..."
        installKVMPackages

	echo "Untarring Resources ..."
        untarResources

	if [ "$DEPLOYMENT_TYPE" == "vm" ]
	then
		echo "Validating installation ... "
		validate
	fi

	echo "Creating VRTM startup scripts"
	createvRTMStartScript
	
	echo "Installing log4cpp library..."
        install_log4cpp
	
	echo "Creating Log directory for VRTM..."
	mkdir -p "$LOG_DIR"
	chmod 777 "$LOG_DIR"

	echo "Installing vrtmcore ..."

	echo "Updating ldconfig for vRTM library"
	echo "$INSTALL_DIR/vrtm/lib" > /etc/ld.so.conf.d/vrtm.conf
	ldconfig

	if [ "$DEPLOYMENT_TYPE" == "vm" ]
	then
		echo "Installing vrtmProxy..."
		installvrtmProxy
		touch "$LOG_DIR"/vrtm_proxy.log
        	chmod 766 "$LOG_DIR"/vrtm_proxy.log
	fi
 
	startNonTPMRpCore

    #verifier symlink
    tbootxmVerifier="/opt/tbootxm/bin/verifier"
    vrtmVerifier="$INSTALL_DIR/vrtm/bin/verifier"
    if [ ! -f "$tbootxmVerifier" ]; then
      echo "Could not find $tbootxmVerifier"
    fi
    if [ -f "$vrtmVerifier" ]; then
      rm -f "$vrtmVerifier"
    fi
    ln -s "$tbootxmVerifier" "$vrtmVerifier"
	
    echo "Install completed successfully !"
}

function help_display()
{
	echo "Usage : ./vRTM_KVM_install.sh [Options]"
        echo "This script creates the installer tar for vrtmCore"
        echo "    default : Installs vrtmCore components"
	echo "			 packaged along with vRTM dist"
	exit
}


if [ "$#" -gt 1 ] ; then
	help_display
fi

if [ "$1" == "--help" ] ; then
       help_display
elif [ "$1" == "--with-libvirt" ] ; then
	BUILD_LIBVIRT="TRUE"
	main_default
else
	echo "Installing vrtmCore components"
	main_default
fi


