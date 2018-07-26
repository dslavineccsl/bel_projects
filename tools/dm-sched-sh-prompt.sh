 _dm-sched()
{


    local cur prev cmdlist
  	COMPREPLY=()
    cur="${COMP_WORDS[COMP_CWORD]}"
    prev="${COMP_WORDS[COMP_CWORD-1]}"


    local cur prev devlist dev cmdlist cmd
    COMPREPLY=()
    cur="${COMP_WORDS[COMP_CWORD]}"
    prev="${COMP_WORDS[COMP_CWORD-1]}"
    compline="${COMP_WORDS[*]}"
    dev=`echo "$compline" | grep -o -E "(dev|tcp|udp)\/.[a-zA-Z0-9\_\-\.]* "`

    cmdlist="add remove keep overwrite clear status dump chkrem"
    cmdpattern="(add|remove|keep|overwrite|clear|status|dump|chkrem) "
    cmd=`echo "$compline" | grep -o -E "$cmdpattern" | cut -d\  -f1`


    if [ -z $dev ]; then #1. no eb-device, find one
      devlist=`history | cut -c 8- | grep -v "grep" | grep "\(dm\-\)[a-zA-Z0-9\-\_]*" | grep -o -E "(dev|tcp|udp)\/.[a-zA-Z0-9\_\-\.]*"`
      COMPREPLY=( $(compgen -W "${devlist}" -- $cur) )
    elif [ -z $cmd ]; then # 2. eb-dev, but no cmd. find one
        COMPREPLY=( $(compgen -W "${cmdlist}" -- $cur) )
    elif [ -n "$dev" ] || [ -n "$cmd" ]; then # 3. eb-dev and cmd, get file if necessary
  	  case "$cmd" in
       	add|remove|keep|overwrite|chkrem)
  	   		COMPREPLY=( $( compgen -f -X '!*.dot' -- $cur ) )
    		;;
  	    *)
  	    ;;
  	  esac
  	fi

    return 0

}
complete -o filenames -F _dm-sched dm-sched