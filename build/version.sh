srcdir="$1"

# If no git repo try to read from the existing git_version.h, for building from tarballs
if ! test -d "${srcdir}/.git"; then
  version_h_path="${srcdir}/build/git_version.h"
  if test -f "${version_h_path}"; then
    while read line; do
      set -- $line
      export $2=$(echo $3 | sed 's/"//g')
    done < "${version_h_path}"
    if test x$BUILD_GIT_VERSION_NUMBER != x -a x$BUILD_GIT_VERSION_STRING != x; then
      export VERSION_SOURCE="from cached git_version.h"
      return 0
    else
      echo "invalid git_version.h"
      exit 2
    fi
  else
    echo "git repo not found and no cached git_version.h"
    exit 2
  fi
fi

last_svn_revision=6962
last_svn_hash="16cd907fe7482cb54a7374cd28b8501f138116be"

git_revision=$(expr $last_svn_revision + $(git log --pretty=oneline $last_svn_hash..HEAD 2>/dev/null | wc -l))
git_version_str=$(git describe --exact-match 2> /dev/null)
git_version_postfix=0
if test x$git_version_str = x; then
  git_version_str=$(git describe --tags $(git rev-list --tags --max-count=1 2> /dev/null) 2> /dev/null)
  git_version_postfix=1
fi
git_version_date=$(git log HEAD -1 --format="%at" 2> /dev/null | xargs -I{} date -d @{} +%Y-%m-%d 2> /dev/null)
installer_version='0.0.0'
resource_version='0, 0, 0'
if test x$git_version_str != x; then
  git_version_str="${git_version_str##v}"
  tagged_release=1
  if [ $(echo $git_version_str | grep '\d\.\d\.\d') ]; then
    installer_version=$git_version_str
    resource_version=$(echo $git_version_str | sed 's/\./, /g')
  fi
else
  git_branch="$(git symbolic-ref HEAD 2> /dev/null)" || git_branch="(unnamed branch)"
  git_branch="${git_branch##refs/heads/}"
  git_hash=$(git rev-parse --short HEAD)

  git_version_str="${git_revision}-${git_branch}-${git_hash}"
  tagged_release=0
fi


if test x$git_version_postfix != x0; then
  git_version_str="${git_version_str}+r${git_revision}"
fi


new_version_h="\
#define BUILD_GIT_VERSION_NUMBER ${git_revision}
#define BUILD_GIT_VERSION_STRING \"${git_version_str}\"
#define TAGGED_RELEASE ${tagged_release}
#define INSTALLER_VERSION \"${installer_version}\"
#define RESOURCE_BASE_VERSION ${resource_version}"

# may not exist yet for out of tree builds
mkdir -p build
version_h_path="build/git_version.h"

# Write it only if it's changed to avoid spurious rebuilds
# This bizzare comparison method is due to that newlines in shell variables are very exciting
case "$(cat ${version_h_path} 2> /dev/null)"
in
  "${new_version_h}");;
  *) echo "${new_version_h}" > "${version_h_path}"
esac

export BUILD_GIT_VERSION_NUMBER="${git_revision}"
export BUILD_GIT_VERSION_STRING="${git_version_str}"
export VERSION_SOURCE="from git"

cat << EOF > build/git_version.xml
<?xml version="1.0" encoding="utf-8"?>
<Project ToolsVersion="12.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup>
    <GitVersionNumber>${git_revision}</GitVersionNumber>
    <GitVersionString>${git_version_str}</GitVersionString>
  </PropertyGroup>
</Project>
EOF

cat << EOF > build/git_version.json
{
  "BUILD_GIT_VERSION_NUMBER": "${git_revision}",
  "BUILD_GIT_VERSION_STRING": "${git_version_str}",
  "BUILD_GIT_VERSION_DATE": "${git_version_date}",
  "TAGGED_RELEASE": "${tagged_release}",
  "INSTALLER_VERSION": "${installer_version}",
  "RESOURCE_BASE_VERSION": "${resource_version}"
}
EOF
