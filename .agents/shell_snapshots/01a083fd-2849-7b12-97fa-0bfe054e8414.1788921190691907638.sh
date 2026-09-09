# Snapshot file
# Unset all aliases to avoid conflicts with functions
# Functions
gawklibpath_append () 
{ 
    [ -z "$AWKLIBPATH" ] && AWKLIBPATH=`gawk 'BEGIN {print ENVIRON["AWKLIBPATH"]}'`;
    export AWKLIBPATH="$AWKLIBPATH:$*"
}
gawklibpath_default () 
{ 
    unset AWKLIBPATH;
    export AWKLIBPATH=`gawk 'BEGIN {print ENVIRON["AWKLIBPATH"]}'`
}
gawklibpath_prepend () 
{ 
    [ -z "$AWKLIBPATH" ] && AWKLIBPATH=`gawk 'BEGIN {print ENVIRON["AWKLIBPATH"]}'`;
    export AWKLIBPATH="$*:$AWKLIBPATH"
}
gawkpath_append () 
{ 
    [ -z "$AWKPATH" ] && AWKPATH=`gawk 'BEGIN {print ENVIRON["AWKPATH"]}'`;
    export AWKPATH="$AWKPATH:$*"
}
gawkpath_default () 
{ 
    unset AWKPATH;
    export AWKPATH=`gawk 'BEGIN {print ENVIRON["AWKPATH"]}'`
}
gawkpath_prepend () 
{ 
    [ -z "$AWKPATH" ] && AWKPATH=`gawk 'BEGIN {print ENVIRON["AWKPATH"]}'`;
    export AWKPATH="$*:$AWKPATH"
}

# setopts 3
set -o braceexpand
set -o hashall
set -o interactive-comments

# aliases 0

# exports 73
declare -x CLUTTER_IM_MODULE="fcitx"
declare -x CODEX_HOME="/home/workspace_data/works/myprojects/wise_combine_test.plan_goal/.agents"
declare -x CODEX_MANAGED_BY_NPM="1"
declare -x CODEX_MANAGED_PACKAGE_ROOT="/home/hpvr/.nvm/versions/node/v22.23.2/lib/node_modules/@openai/codex"
declare -x COLORTERM="truecolor"
declare -x DBUS_SESSION_BUS_ADDRESS="unix:path=/run/user/1000/bus"
declare -x DESKTOP_SESSION="ubuntu"
declare -x DISPLAY=":0"
declare -x GDMSESSION="ubuntu"
declare -x GIO_LAUNCHED_DESKTOP_FILE="/usr/share/applications/terminator.desktop"
declare -x GIO_LAUNCHED_DESKTOP_FILE_PID="17914"
declare -x GJS_DEBUG_OUTPUT="stderr"
declare -x GJS_DEBUG_TOPICS="JS ERROR;JS LOG"
declare -x GNOME_DESKTOP_SESSION_ID="this-is-deprecated"
declare -x GNOME_SHELL_SESSION_MODE="ubuntu"
declare -x GPG_AGENT_INFO="/run/user/1000/gnupg/S.gpg-agent:0:1"
declare -x GTK_IM_MODULE="fcitx"
declare -x GTK_MODULES="gail:atk-bridge"
declare -x HOME="/home/hpvr"
declare -x IM_CONFIG_PHASE="1"
declare -x INVOCATION_ID="3b6b166a7a2e47f68ce4dcaeafbf16fb"
declare -x JOURNAL_STREAM="8:69657"
declare -x LANG="zh_CN.UTF-8"
declare -x LANGUAGE="zh_CN:zh:en_US:en"
declare -x LC_ADDRESS="zh_CN.UTF-8"
declare -x LC_IDENTIFICATION="zh_CN.UTF-8"
declare -x LC_MEASUREMENT="zh_CN.UTF-8"
declare -x LC_MONETARY="zh_CN.UTF-8"
declare -x LC_NAME="zh_CN.UTF-8"
declare -x LC_NUMERIC="zh_CN.UTF-8"
declare -x LC_PAPER="zh_CN.UTF-8"
declare -x LC_TELEPHONE="zh_CN.UTF-8"
declare -x LC_TIME="zh_CN.UTF-8"
declare -x LESSCLOSE="/usr/bin/lesspipe %s %s"
declare -x LESSOPEN="| /usr/bin/lesspipe %s"
declare -x LOGNAME="hpvr"
declare -x LS_COLORS="rs=0:di=01;34:ln=01;36:mh=00:pi=40;33:so=01;35:do=01;35:bd=40;33;01:cd=40;33;01:or=40;31;01:mi=00:su=37;41:sg=30;43:ca=30;41:tw=30;42:ow=34;42:st=37;44:ex=01;32:*.tar=01;31:*.tgz=01;31:*.arc=01;31:*.arj=01;31:*.taz=01;31:*.lha=01;31:*.lz4=01;31:*.lzh=01;31:*.lzma=01;31:*.tlz=01;31:*.txz=01;31:*.tzo=01;31:*.t7z=01;31:*.zip=01;31:*.z=01;31:*.dz=01;31:*.gz=01;31:*.lrz=01;31:*.lz=01;31:*.lzo=01;31:*.xz=01;31:*.zst=01;31:*.tzst=01;31:*.bz2=01;31:*.bz=01;31:*.tbz=01;31:*.tbz2=01;31:*.tz=01;31:*.deb=01;31:*.rpm=01;31:*.jar=01;31:*.war=01;31:*.ear=01;31:*.sar=01;31:*.rar=01;31:*.alz=01;31:*.ace=01;31:*.zoo=01;31:*.cpio=01;31:*.7z=01;31:*.rz=01;31:*.cab=01;31:*.wim=01;31:*.swm=01;31:*.dwm=01;31:*.esd=01;31:*.jpg=01;35:*.jpeg=01;35:*.mjpg=01;35:*.mjpeg=01;35:*.gif=01;35:*.bmp=01;35:*.pbm=01;35:*.pgm=01;35:*.ppm=01;35:*.tga=01;35:*.xbm=01;35:*.xpm=01;35:*.tif=01;35:*.tiff=01;35:*.png=01;35:*.svg=01;35:*.svgz=01;35:*.mng=01;35:*.pcx=01;35:*.mov=01;35:*.mpg=01;35:*.mpeg=01;35:*.m2v=01;35:*.mkv=01;35:*.webm=01;35:*.ogm=01;35:*.mp4=01;35:*.m4v=01;35:*.mp4v=01;35:*.vob=01;35:*.qt=01;35:*.nuv=01;35:*.wmv=01;35:*.asf=01;35:*.rm=01;35:*.rmvb=01;35:*.flc=01;35:*.avi=01;35:*.fli=01;35:*.flv=01;35:*.gl=01;35:*.dl=01;35:*.xcf=01;35:*.xwd=01;35:*.yuv=01;35:*.cgm=01;35:*.emf=01;35:*.ogv=01;35:*.ogx=01;35:*.aac=00;36:*.au=00;36:*.flac=00;36:*.m4a=00;36:*.mid=00;36:*.midi=00;36:*.mka=00;36:*.mp3=00;36:*.mpc=00;36:*.ogg=00;36:*.ra=00;36:*.wav=00;36:*.oga=00;36:*.opus=00;36:*.spx=00;36:*.xspf=00;36:"
declare -x MANAGERPID="1742"
declare -x NVM_BIN="/home/hpvr/.nvm/versions/node/v22.23.2/bin"
declare -x NVM_CD_FLAGS=""
declare -x NVM_DIR="/home/hpvr/.nvm"
declare -x NVM_INC="/home/hpvr/.nvm/versions/node/v22.23.2/include/node"
declare -x PATH="/home/hpvr/.local/bin:/home/hpvr/.local/bin:/home/hpvr/bin:/home/workspace_data/works/myprojects/wise_combine_test.plan_goal/.agents/tmp/arg0/codex-arg001MnvI:/home/hpvr/.nvm/versions/node/v22.23.2/lib/node_modules/@openai/codex/node_modules/@openai/codex-linux-x64/vendor/x86_64-unknown-linux-musl/codex-path:/home/hpvr/.local/bin:/home/hpvr/.pyenv/plugins/pyenv-virtualenv/shims:/home/hpvr/.pyenv/shims:/home/hpvr/.pyenv/bin:/home/hpvr/.nvm/versions/node/v22.23.2/bin:/home/hpvr/bin:/home/hpvr/.local/bin:/home/hpvr/.local/bin:/home/hpvr/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/games:/usr/local/games:/snap/bin:/home/hpvr/works/my_env:/home/hpvr/works/parasoft_cpptest_professional-2023.2.1-linux.x86_64/cpptest:/home/hpvr/works/parasoft_cpptest_professional-2023.2.1-linux.x86_64/cpptest/bin"
declare -x PYENV_ROOT="/home/hpvr/.pyenv"
declare -x PYENV_SHELL="bash"
declare -x PYENV_VIRTUALENV_INIT="1"
declare -x QT4_IM_MODULE="fcitx"
declare -x QT_ACCESSIBILITY="1"
declare -x QT_IM_MODULE="fcitx"
declare -x SESSION_MANAGER="local/hpvr4:@/tmp/.ICE-unix/1899,unix/hpvr4:/tmp/.ICE-unix/1899"
declare -x SHELL="/bin/bash"
declare -x SHLVL="2"
declare -x SSH_AGENT_PID="1856"
declare -x SSH_AUTH_SOCK="/run/user/1000/keyring/ssh"
declare -x TERM="xterm-256color"
declare -x TERMINATOR_DBUS_NAME="net.tenshu.Terminator21a9d5db22c73a993ff0b42f64b396873"
declare -x TERMINATOR_DBUS_PATH="/net/tenshu/Terminator2"
declare -x TERMINATOR_UUID="urn:uuid:349800f3-c345-4a21-9849-97ddd5b73657"
declare -x USER="hpvr"
declare -x USERNAME="hpvr"
declare -x VTE_VERSION="6003"
declare -x WEKNORA_MCP_TOKEN="wmcp_IQBIEymeOkV8nY9QwGHKIOEEDHYPxlbvblN5YHEAX_w"
declare -x WINDOWPATH="2"
declare -x XAUTHORITY="/run/user/1000/gdm/Xauthority"
declare -x XDG_CONFIG_DIRS="/etc/xdg/xdg-ubuntu:/etc/xdg"
declare -x XDG_CURRENT_DESKTOP="ubuntu:GNOME"
declare -x XDG_DATA_DIRS="/usr/share/ubuntu:/usr/local/share/:/usr/share/:/var/lib/snapd/desktop:/opt/apps/com.alibabainc.dingtalk/entries:/opt/apps/com.qq.weixin.deepin/entries:/opt/apps/com.alibabainc.dingtalk/entries:/opt/apps/com.qq.weixin.deepin/entries"
declare -x XDG_MENU_PREFIX="gnome-"
declare -x XDG_RUNTIME_DIR="/run/user/1000"
declare -x XDG_SESSION_CLASS="user"
declare -x XDG_SESSION_DESKTOP="ubuntu"
declare -x XDG_SESSION_TYPE="x11"
declare -x XMODIFIERS="@im=fcitx"
