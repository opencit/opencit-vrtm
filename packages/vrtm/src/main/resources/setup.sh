#!/bin/sh

# VRTM install script
# Outline:
# 1.  load existing environment configuration
# 2.  source the "functions.sh" file:  mtwilson-linux-util-*.sh
# 3.  look for ~/vrtm.env and source it if it's there
# 4.  force root user installation
# 5.  define application directory layout 
# 6.  backup current configuration and data, if they exist
# 7.  create application directories and set folder permissions
# 8.  store directory layout in env file
# 9.  install prerequisites
# 10. unzip vrtm archive vrtm-zip-*.zip into VRTM_HOME, overwrite if any files already exist
# 11. copy utilities script file to application folder
# 12. set additional permissions
# 13. run additional setup tasks

#####

# default settings
# note the layout setting is used only by this script
# and it is not saved or used by the app script
export VRTM_HOME=${VRTM_HOME:-/opt/vrtm}
VRTM_LAYOUT=${VRTM_LAYOUT:-home}
VRTM_VERSION_FILE=vrtm.version
DEFAULT_DEPLOYMENT_TYPE="vm"
if [ -z "$DEPLOYMENT_TYPE" ]
then
	export DEPLOYMENT_TYPE=$DEFAULT_DEPLOYMENT_TYPE
fi

# the env directory is not configurable; it is defined as VRTM_HOME/env and
# the administrator may use a symlink if necessary to place it anywhere else
export VRTM_ENV=$VRTM_HOME/env

# load application environment variables if already defined
if [ -d $VRTM_ENV ]; then
  VRTM_ENV_FILES=$(ls -1 $VRTM_ENV/*)
  for env_file in $VRTM_ENV_FILES; do
    . $env_file
    env_file_exports=$(cat $env_file | grep -E '^[A-Z0-9_]+\s*=' | cut -d = -f 1)
    if [ -n "$env_file_exports" ]; then eval export $env_file_exports; fi
  done
fi

# functions script (mtwilson-linux-util-3.0-SNAPSHOT.sh) is required
# we use the following functions:
# java_detect java_ready_report 
# echo_failure echo_warning
# register_startup_script
UTIL_SCRIPT_FILE=$(ls -1 mtwilson-linux-util-*.sh | head -n 1)
if [ -n "$UTIL_SCRIPT_FILE" ] && [ -f "$UTIL_SCRIPT_FILE" ]; then
  . $UTIL_SCRIPT_FILE
fi

# load installer environment file, if present
if [ -f ~/vrtm.env ]; then
  echo "Loading environment variables from $(cd ~ && pwd)/vrtm.env"
  . ~/vrtm.env
  env_file_exports=$(cat ~/vrtm.env | grep -E '^[A-Z0-9_]+\s*=' | cut -d = -f 1)
  if [ -n "$env_file_exports" ]; then eval export $env_file_exports; fi
else
  echo "No environment file"
fi

# enforce root user installation
if [ "$(whoami)" != "root" ]; then
  echo_failure "Running as $(whoami); must install as root"
  exit -1
fi

# define application directory layout
if [ "$VRTM_LAYOUT" == "linux" ]; then
  export VRTM_CONFIGURATION=${VRTM_CONFIGURATION:-/etc/vrtm}
  export VRTM_REPOSITORY=${VRTM_REPOSITORY:-/var/opt/vrtm}
  export VRTM_LOGS=${VRTM_LOGS:-/var/log/vrtm}
elif [ "$VRTM_LAYOUT" == "home" ]; then
  export VRTM_CONFIGURATION=${VRTM_CONFIGURATION:-$VRTM_HOME/configuration}
  export VRTM_REPOSITORY=${VRTM_REPOSITORY:-$VRTM_HOME/repository}
  export VRTM_LOGS=${VRTM_LOGS:-/var/log/vrtm}
#  export VRTM_LOGS=${VRTM_LOGS:-$VRTM_HOME/logs}
fi
export VRTM_BIN=$VRTM_HOME/bin
export VRTM_JAVA=$VRTM_HOME/java

# note that the env dir is not configurable; it is defined as "env" under home
export VRTM_ENV=$VRTM_HOME/env

vrtm_backup_configuration() {
  if [ -n "$VRTM_CONFIGURATION" ] && [ -d "$VRTM_CONFIGURATION" ] &&
    (find "$VRTM_CONFIGURATION" -mindepth 1 -print -quit | grep -q .); then
    datestr=`date +%Y%m%d.%H%M`
    backupdir=/var/backup/vrtm.configuration.$datestr
    mkdir -p "$backupdir"
    cp -r $VRTM_CONFIGURATION $backupdir
  fi
}

vrtm_backup_repository() {
  if [ -n "$VRTM_REPOSITORY" ] && [ -d "$VRTM_REPOSITORY" ] &&
    (find "$VRTM_REPOSITORY" -mindepth 1 -print -quit | grep -q .); then
    datestr=`date +%Y%m%d.%H%M`
    backupdir=/var/backup/vrtm.repository.$datestr
    mkdir -p "$backupdir"
    cp -r $VRTM_REPOSITORY $backupdir
  fi
}

# backup current configuration and data, if they exist
vrtm_backup_configuration
vrtm_backup_repository

# create application directories (chown will be repeated near end of this script, after setup)
for directory in $VRTM_HOME $VRTM_CONFIGURATION $VRTM_REPOSITORY $VRTM_JAVA $VRTM_BIN $VRTM_LOGS $VRTM_ENV; do
  mkdir -p $directory
  chmod 700 $directory
done

# store directory layout in env file
echo "# $(date)" > $VRTM_ENV/vrtm-layout
echo "export VRTM_HOME=$VRTM_HOME" >> $VRTM_ENV/vrtm-layout
echo "export VRTM_CONFIGURATION=$VRTM_CONFIGURATION" >> $VRTM_ENV/vrtm-layout
echo "export VRTM_REPOSITORY=$VRTM_REPOSITORY" >> $VRTM_ENV/vrtm-layout
echo "export VRTM_JAVA=$VRTM_JAVA" >> $VRTM_ENV/vrtm-layout
echo "export VRTM_BIN=$VRTM_BIN" >> $VRTM_ENV/vrtm-layout
echo "export VRTM_LOGS=$VRTM_LOGS" >> $VRTM_ENV/vrtm-layout

# install prerequisites
VRTM_YUM_PACKAGES="zip unzip xmlstarlet"
VRTM_APT_PACKAGES="zip unzip xmlstarlet"
VRTM_YAST_PACKAGES="zip unzip xmlstarlet"
VRTM_ZYPPER_PACKAGES="zip unzip xmlstarlet"
auto_install "Installer requirements" "VRTM"
if [ $? -ne 0 ]; then echo_failure "Failed to install prerequisites through package installer"; exit -1; fi

# delete existing java files, to prevent a situation where the installer copies
# a newer file but the older file is also there
if [ -d $VRTM_HOME/java ]; then
  rm $VRTM_HOME/java/*.jar 2>/dev/null
fi

# extract vrtm  (vrtm-zip-0.1-SNAPSHOT.zip)
echo "Extracting application..."
VRTM_ZIPFILE=`ls -1 vrtm-*.zip 2>/dev/null | head -n 1`
unzip -oq $VRTM_ZIPFILE -d $VRTM_HOME

# copy utilities script file to application folder
cp $UTIL_SCRIPT_FILE $VRTM_HOME/bin/functions.sh

# set permissions
chmod 700 $VRTM_HOME/bin/*.sh
chmod 700 $VRTM_HOME/dist/*.sh

(cd $VRTM_HOME/dist && ./vRTM_KVM_install.sh)
cp $VRTM_HOME/dist/$VRTM_VERSION_FILE $VRTM_HOME/.

rm -rf /$VRTM_HOME/dist

#Register vRTM start script
register_startup_script /usr/local/bin/vrtm vrtm

# if [ "$DEPLOYMENT_TYPE" == "vm" ]
# then
	# register_startup_script /usr/local/bin/vrtmlistener vrtmlistener
# fi

#Update vRTM.cfg file to include deployment_type
vrtmCfgFile=$VRTM_CONFIGURATION/vRTM.cfg
vrtmCfgDeploymentTypeExists=$(grep '^deployment_type' "$vrtmCfgFile")
if [ -n "$vrtmCfgDeploymentTypeExists" ]; then
  sed -i 's/^deployment_type.*/deployment_type='$DEPLOYMENT_TYPE'/g' "$vrtmCfgFile"
else
  echo "deployment_type=$DEPLOYMENT_TYPE" >> "$vrtmCfgFile"
fi

### CURRENTLY DONE IN vRTM_KVM_install.sh
##verifier
#tbootxmVerifier="/opt/tbootxm/bin/verifier"
#vrtmVerifier="$INSTALL_DIR/rpcore/bin/debug/verifier"
#if [ ! -f "$tbootxmVerifier" ]; then
#  echo_warning "Could not find $tbootxmVerifier"
#fi
#if [ -f "$vrtmVerifier" ]; then
#  rm -f "$vrtmVerifier"
#fi
#ln -s "$tbootxmVerifier" "$vrtmVerifier"

ldconfig
if [ $? -ne 0 ]; then echo_warning "Failed to load ldconfig. Please run command "ldconfig" after installation completes. And start vrtm service again"; fi

echo_success "VRTM Installation complete"
