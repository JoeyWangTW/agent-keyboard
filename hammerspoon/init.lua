-- List of devices you don't want to auto-connect to (You can modify this list)
local unwantedDevices = {"WH-1000XM4"} -- Replace with actual device names

-- Global variable to store the last known "wanted" device
local lastWantedDevice = hs.audiodevice.defaultOutputDevice():name()

-- Function to check if a device is unwanted
local function isUnwantedDevice(deviceName)
    for _, unwantedDevice in ipairs(unwantedDevices) do
        if deviceName == unwantedDevice then
            return true
        end
    end
    return false
end

local function watchForAudioDeviceChanges(eventType)
    local currentDevice = hs.audiodevice.defaultOutputDevice()

    -- If the current device is unwanted and the event is not "dOut"
    if isUnwantedDevice(currentDevice:name()) and eventType ~= "dOut" then
        if lastWantedDevice then
            local device = hs.audiodevice.findOutputByName(lastWantedDevice)
            if device then
                device:setDefaultOutputDevice()
            end
        end
    else
        -- Update the lastWantedDevice if the current device is not unwanted
        -- or if it was manually selected (eventType == "dOut")
        lastWantedDevice = currentDevice:name()
    end
end

-- Initialize and start the watcher
local DeviceWatcher = hs.audiodevice.watcher
hs.audiodevice.watcher.setCallback(watchForAudioDeviceChanges)
DeviceWatcher:start()


-- ── Window Grid (KMK Keyboard) ──
-- Ctrl+Alt+Cmd+1‑8 snaps focused window to 2×4 grid on MacBook screen
--   [1] [2] [3] [4]
--   [5] [6] [7] [8]

-- Use the built-in display as the target screen
-- hs.screen.primaryScreen() returns the screen with the menu bar
local function getMacScreen()
  return hs.screen.primaryScreen()
end

local grid = {
  -- Row 1
  {x=0,    y=0,   w=0.25, h=0.5},  -- key 1: top-left
  {x=0.25, y=0,   w=0.25, h=0.5},  -- key 2: top-center-left
  {x=0.5,  y=0,   w=0.25, h=0.5},  -- key 3: top-center-right
  {x=0.75, y=0,   w=0.25, h=0.5},  -- key 4: top-right
  -- Row 2
  {x=0,    y=0.5, w=0.25, h=0.5},  -- key 5: bottom-left
  {x=0.25, y=0.5, w=0.25, h=0.5},  -- key 6: bottom-center-left
  {x=0.5,  y=0.5, w=0.25, h=0.5},  -- key 7: bottom-center-right
  {x=0.75, y=0.5, w=0.25, h=0.5},  -- key 8: bottom-right
}

local mods = {"ctrl", "alt", "cmd"}

for i, rect in ipairs(grid) do
  hs.hotkey.bind(mods, tostring(i), function()
    local win = hs.window.focusedWindow()
    if not win then return end
    local screen = getMacScreen()
    if screen then
      win:moveToScreen(screen)
      win:moveToUnit(rect, 0)
    end
  end)
end


-- ── Cycle Focus Through Grid Windows ──
-- Ctrl+Alt+Cmd+0 focuses the next window occupying a grid cell (1→2→…→8→1)
-- Uses a window filter to avoid expensive hs.window.orderedWindows() calls

local wf = hs.window.filter.new():setCurrentSpace(true):setDefaultFilter({})


-- ── Focus Window at Grid Position ──
-- Ctrl+Alt+Cmd+Shift+1‑8 focuses the window occupying that grid cell
local focusMods = {"ctrl", "alt", "cmd", "shift"}

for i, rect in ipairs(grid) do
  hs.hotkey.bind(focusMods, tostring(i), function()
    local screen = getMacScreen()
    if not screen then return end
    local sf = screen:frame()
    local allWindows = wf:getWindows(hs.window.filter.sortByFocusedLast)

    local tx = sf.x + rect.x * sf.w
    local ty = sf.y + rect.y * sf.h
    local tw = rect.w * sf.w
    local th = rect.h * sf.h

    for _, win in ipairs(allWindows) do
      local f = win:frame()
      local cx = f.x + f.w / 2
      local cy = f.y + f.h / 2
      if cx >= tx and cx < tx + tw and cy >= ty and cy < ty + th then
        win:focus()
        return
      end
    end
  end)
end


local cycleIndex = 0

hs.hotkey.bind(mods, "0", function()
  local screen = getMacScreen()
  if not screen then return end
  local sf = screen:frame()
  local allWindows = wf:getWindows(hs.window.filter.sortByFocusedLast)

  for attempt = 1, #grid do
    cycleIndex = (cycleIndex % #grid) + 1
    local r = grid[cycleIndex]

    -- Target cell bounds in absolute coordinates
    local tx = sf.x + r.x * sf.w
    local ty = sf.y + r.y * sf.h
    local tw = r.w * sf.w
    local th = r.h * sf.h

    for _, win in ipairs(allWindows) do
      local f = win:frame()
      local cx = f.x + f.w / 2
      local cy = f.y + f.h / 2
      if cx >= tx and cx < tx + tw and cy >= ty and cy < ty + th then
        win:focus()
        return
      end
    end
  end
end)


-- ── Move to Screen 2 + Maximize (KMK Layer 1) ──
-- Ctrl+Alt+Cmd+9 moves focused window to second screen and maximizes it

hs.hotkey.bind(mods, "9", function()
  local win = hs.window.focusedWindow()
  if not win then return end
  local screens = hs.screen.allScreens()
  if #screens > 1 then
    win:moveToScreen(screens[2])
    win:maximize(0)
  end
end)


-- ── Rotary Encoder Scroll (SU120 Keyboard) ──
-- Ctrl+Alt+Cmd+Up/Down from the right encoder → mouse scroll events
local scrollAmount = 5

hs.hotkey.bind(mods, "up", function()
  hs.eventtap.scrollWheel({0, scrollAmount}, {})
end)

hs.hotkey.bind(mods, "down", function()
  hs.eventtap.scrollWheel({0, -scrollAmount}, {})
end)
