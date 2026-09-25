# Release Control - open-source Apple Release Control framework

LibreDarwin clean-room reimplementation of Apple's Release Control
build framework for Darwin Core OS projects.  Not affiliated with
Apple.

## Layout

The tree mirrors Apple's Core OS Makefiles layout so that Darwin
projects built against `$(MAKEFILEPATH)/CoreOS` work unmodified:

- `ReleaseControl/` - the Release Control makefile API
  (`Common.make`, `BSDCommon.make`, `GNUSource.make`)
- `Standard/` - the standard command and variable sets.  `Commands.in`
  and `Variables.in` are dual-flavor (GNU make and BSD make) and are
  turned into `.make` files with `unifdef` at install time.
- `Xcode/` - `BSD.xcconfig` for Xcode-aliased projects

## Install

`make` (GNU make, as shipped with Xcode) or BSD `make`:

```
make install DSTROOT=/path/to/dst DEVELOPMENT_DIR=/usr  # from this dir
```

Drops the framework into `$(DSTROOT)$(DEVELOPER_DIR)/Makefiles/CoreOS`.
Projects then `include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make`.

## License

BSD-3-Clause.  See `LICENSE`.