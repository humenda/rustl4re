#! /usr/bin/env bash
#
# Convert relative path to absolute one
#
# Adam Lackorzynski <adam@os.inf.tu-dresden.de>
#

help()
{
  echo PWD=\$PWD $0 relpath1 [relpath2 [..]]
  exit $1
}

convertpath()
{
  relpath=$1
  basepath=$PWD
  # sanity checks
  [ -z "$relpath" -o -z "$basepath" ] && help 1
  [ "${basepath#/}" = "${basepath}" ] && help 1
  [ "${basepath/\/..\//}" = "${basepath}" ] || help 1
  [ "${basepath/\/.\//}" = "${basepath}" ] || help 1
  [ "${basepath/%\/../}" = "${basepath}" ] || help 1
  [ "${basepath/%\/./}" = "${basepath}" ] || help 1


  # remove slashes at the end
  while [ "${relpath%/}" != "${relpath}" ];
  do relpath="${relpath%/}"; done

  # remove double/multi slashes
  while [ "${relpath/\/\///}" != "${relpath}" ];
  do relpath=${relpath/\/\///}; done

  # is relpath relative?
  if [ "${relpath#/}" != "${relpath}" ]; then
    basepath=''
    relpath=${relpath#/}
  fi

  relpath="$relpath/"

  while [ -n "$relpath" ];
  do
    elem=${relpath%%/*}
    relpath=${relpath#*/}

    case $elem in
      .) # skip
	;;
      ..)
	 basepath=${basepath%/*}
	;;
      *)
	 basepath=$basepath/$elem
	;;
    esac

  done

  [ -z "$basepath" ] && basepath=/$basepath

  echo $basepath
}

while [ -n "$1" ];
do
  convertpath $1
  shift
done

exit 0
