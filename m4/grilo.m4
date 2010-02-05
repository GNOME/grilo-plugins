dnl Outputs configuration summary
AC_DEFUN([AG_GRL_OUTPUT_PLUGINS], [

printf "\n"
printf -- "----------------- Configuration summary -----------------\n\n"
( for i in $GRL_PLUGINS_ALL; do
    case " $GRL_PLUGINS_ENABLED " in
      *\ $i\ *)
        printf '\t'$i': yes\n'
        ;;
      *)
        printf '\t'$i': no\n'
        ;;
    esac
  done ) | sort
printf -- "\n---------------------------------------------------------\n"
printf "\n"
])
