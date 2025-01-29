local function of(projectName)
  return "../build/" .. projectName .. "/%{cfg.system}_%{cfg.architecture}/%{cfg.buildcfg}"
end

return {
  setup = function (projectName)
    location  ("../build/" .. projectName)

    local basedir = of(projectName);
    targetdir (basedir)
    objdir    (basedir .. "/obj")
  end,
  of = of
}
