# Getting started

This page provides an overview of the first steps in using the Dragon Age toolset, and some information that is good to have before embarking on your journey to modding *Dragon Age: Origins*.

!!! warning "This article contains links to the BioWare Social Network (BSN), which is now closed."

    These links should be replaced with working links where possible, and tutorials edited to remove reliance on BSN.

## First steps

1. **[Install the game.](1_install_game.md)**

    Ensure that Dragon Age is installed before dabbling in the toolset, as it relies on the game's resources and engine, and cannot function without them.

1. **[Install and troubleshoot the toolset.](2_install_toolset.md)**

    The Dragon Age toolset contains the same functionality used by BioWare to craft the official campaign, and provides the power and flexibility to create your own custom content and adventures.

    Refer to this page for general information and step-by-step instructions to installing the toolset,
    as well as specific information on how to resolve issues in the event that toolset installation goes awry.

1. **[Consult the list of known issues.](3_known_issues.md)**

    Refer to this page for a currently documented list of known issues and bugs in the current version of the toolset.

## Overview of the toolset

- [Overview of the toolset](overview.md): A quite good overview of interface layout and toolset resource management.
- [Comparisons with other toolkits](comparisons.md): A brief feature overview of the Dragon Age toolset for users familiar with similar software (e.g., the NWN Aurora Toolset).

## Starting the toolset

Before using the Toolset, it is important to take note of what [module](/module.md) the toolset has open.

<figure markdown="span">
  ![How to determine which module is currently open](https://www.datoolset.net/mw/images/6/6e/Titlebar_components.png){ width="400" }
  <figcaption>How to determine which module is currently open.</figcaption>
</figure>

A module is a "container" in which game resources are stored in; you will only be able to see the resources for the currently open module (and any modules that it is marked as depending on). The default module when first starting the Toolset is the "Demo" module, which is a very small and simple module intended to show several common resource types in a basic pre-built adventure that you can experiment with safely.

The main Dragon Age: Origins campaign resources are stored in a module named "Single Player". If you're just starting out with the toolset, it can be dangerous to edit these, as you could inadvertently corrupt your main campaign. Take care when exploring not to edit any of those resources without knowing what you're doing.

!!! danger

    Do not edit Single Player or Core Resources with custom content!

The first basic steps in the Toolset shall be to create your own Module. The only possible type of Module is "Addin", but note that this name is somewhat misleading; it is possible to have "standalone addins" that are completely separate from any other module (such as the main campaign).


!!! tip

     See [Module](/module.md) page for a general overview of how a Module works, and what awaits you.

## Using the toolset

As described in the overview linked to above, the game uses a wide variety of resource types. A "resource" is something that you can create, such as a script or a creature, and you will often need to combine many different resources together to accomplish a particular design goal. The toolset consists of many independent tools for working with these different types of resources. These tools can be well discussed separately.

The next best step for learning the toolset would be to look into the categories on the [Main Page](/index.md), which link deeper into each topic and the tools involved with them. If you are having questions at a specific part of the toolset give this Wiki a search for it. This can be very specific, and always worth a try. You may also find the toolset forums to be useful in solving specific problems the documentation here may not cover.

A list of certain Interest, and the 3 Resource types:

  - [Design](/design.md) - Talks mainly about Designer Resources, such as Items, Monsters, NPC's, Vendors, etc.
  - [Art](/art.md) - Talks mainly about Art Resources, such as Models, Textures, Facemorphs, etc.
  - [2DA](/2da.md) - Talks about the 2DA Resourcetype, which is sets of Gamewide usable variables and references.

### Tips and tricks

  - [Shortcuts](/shortcuts.md) - Assorted toolset shortcuts.
  - [Common problems](/common_problems.md) - Little things that most people will need to know but that are not necessarily obvious.

!!! bug

    It is currently not possible to delete a module by normal means.

!!! tip

    If you run into a problem that the documentation doesn't cover and that you can't fix, contact toolsetsupport@bioware.com.

### Removing custom content

This section describes how to remove all custom content from the game. Since you can't be certain that all authors have followed best practice, you'll need to clean out all the folders that can be customised.

!!! tip

    To be on the safe side, before you start, you might want to:

      - Disable all the unofficial add-ins on the DLC screen in game.
      - Play a recently saved game (using the Force Load option) to confirm that it works reasonably well without the add-ins.
      - Back up your `Documents > Bioware` folder.

In `Documents > Bioware > Dragon Age`,

  - In the AddIns folder, delete all the folders except `Demo`, `Single Player` and anything starting with `dao_prc` (the official DLCs).
  - Delete any files In the `AddIns > Single Player` folder and sub-folders (you might get away with just deleting the `Single Player` folder, but caution never hurts).
  - Delete any files from the `modules/` and `packages/` folders.

### Dragon Age 2

See [Dragon Age 2](/dae.md) for more information.
