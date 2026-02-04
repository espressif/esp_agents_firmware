from typing import Any
import click
import subprocess
import sys
import os
import re
from pathlib import Path
import shutil

# Constants
BOARD_NAME_FILE = "agent_board_name.txt"
SDKCONFIG_DEFAULTS = "sdkconfig.defaults"
USE_FROM_BMGR_FILE = ".use_from_esp_board_manager"
BOARD_DEVICES_YAML = "board_devices"
BOARD_PERIPHERALS_YAML = "board_peripherals"
BMGR_COMPONENT = "espressif__esp_board_manager"
BMGR_SCRIPT = "gen_bmgr_config_codes.py"

IDF_PATH = os.getenv("IDF_PATH", "")

BOARDS_DIR = Path(__file__).parent.resolve()


class BoardChoice(click.Choice):
    """Custom Choice class with custom error message for board selection."""

    def __init__(self, choices: list[str], case_sensitive: bool = False):
        super().__init__(choices, case_sensitive=case_sensitive)
        self.choices_list = choices

    def convert(self, value: Any, param: Any, ctx: Any) -> Any:
        """Convert and validate the value with custom error message."""
        try:
            return super().convert(value, param, ctx)
        except click.BadParameter:
            boards_list = ', '.join(self.choices_list)
            raise click.BadParameter(
                f"'{value}' is not a valid board name.\n"
                "Did you add the board configation to `agent_firmware/components/boards/<board_name>`?\n\n"
                f"Available boards: {boards_list}"
            ) from None


def _validate_board_path(board_path: Path) -> Path:
    """Validate and normalize board path."""
    if not board_path.exists():
        raise click.BadParameter(f"Board path does not exist: {board_path}")

    if not board_path.is_dir():
        raise click.BadParameter(f"Board path is not a directory: {board_path}")

    return board_path.resolve()


def _write_board_name_file(board_name: str, output_file: Path) -> None:
    """Write the absolute board path to the output file."""
    try:
        output_file.write_text(f"{board_name}\n", encoding="utf-8")
        click.echo(f"Written board name to {output_file}")
    except OSError as e:
        raise click.ClickException(f"Failed to write board path file: {e}")


def _ensure_bmgr_script_exists(bmgr_script_path: Path, project_dir: Path) -> None:
    """Ensure the board manager script exists, initializing if necessary."""
    if bmgr_script_path.exists():
        return

    # Try to initialize by running reconfigure
    click.echo("ESP Board Manager not found. Running `idf.py reconfigure` to initialize...")
    result = subprocess.run(
        [sys.executable, f"{IDF_PATH}/tools/idf.py", "reconfigure"],
        cwd=project_dir,
        check=False
    )

    if result.returncode != 0:
        raise click.ClickException(
            "Failed to initialize ESP Board Manager. "
            "Please run 'idf.py reconfigure`."
        )

    if not bmgr_script_path.exists():
        raise click.ClickException(
            f"gen_bmgr_config_codes.py not found: {bmgr_script_path}. "
            "Please ensure ESP Board Manager component is properly installed."
        )


def _find_yaml_file(board_path: Path, base_name: str) -> Path | None:
    """Find a YAML file with either .yaml or .yml extension."""
    for ext in (".yaml", ".yml"):
        yaml_path = board_path / f"{base_name}{ext}"
        if yaml_path.exists():
            return yaml_path
    return None


def _validate_custom_board_files(board_path: Path) -> None:
    """Validate that required YAML files exist for custom boards."""
    board_devices_yml = _find_yaml_file(board_path, BOARD_DEVICES_YAML)
    board_peripherals_yml = _find_yaml_file(board_path, BOARD_PERIPHERALS_YAML)

    missing_files = []
    if not board_devices_yml:
        missing_files.append("board_devices.yaml")
    if not board_peripherals_yml:
        missing_files.append("board_peripherals.yaml")

    if missing_files:
        raise click.ClickException(
            f"Required files not found in board path '{board_path}'. "
            f"Missing: {', '.join(missing_files)}"
        )


def _run_bmgr_script(
    bmgr_script_path: Path,
    project_dir: Path,
    board_name: str,
    board_path: Path | None = None
) -> None:
    """Run the board manager configuration script."""
    cmd = [sys.executable, str(bmgr_script_path), "-b", board_name]

    if board_path is not None:
        # Custom board path
        cmd.extend(["-c", str(board_path)])
        click.echo(f"Running gen_bmgr_config_codes.py for custom board: {board_name}")
    else:
        # Board from ESP Board Manager
        click.echo(f"Running gen_bmgr_config_codes.py for board: {board_name}")

    result = subprocess.run(cmd, cwd=project_dir, check=False)

    if result.returncode != 0:
        raise click.ClickException(
            f"Failed to run gen_bmgr_config_codes.py. Exit code: {result.returncode}"
        )


def _list_available_boards(boards_dir: Path) -> list[tuple[str, str, bool]]:
    """
    List all available boards in the boards directory.

    Returns:
        List of tuples: (board_name, bmgr_board_name, is_from_bmgr)
        - board_name: The directory name of the board
        - bmgr_board_name: The actual board name used by ESP Board Manager (if applicable)
        - is_from_bmgr: True if board is from ESP Board Manager, False if custom
    """
    boards = []

    if not boards_dir.exists() or not boards_dir.is_dir():
        return boards

    for board_dir in sorted(boards_dir.iterdir()):
        if not board_dir.is_dir():
            continue

        board_name = board_dir.name
        use_from_bmgr_file = board_dir / USE_FROM_BMGR_FILE
        is_from_bmgr = use_from_bmgr_file.exists()
        bmgr_board_name = board_name

        if is_from_bmgr:
            # Try to extract bmgr_board_name from the file
            try:
                file_content = use_from_bmgr_file.read_text(encoding="utf-8")
                match = re.search(r'bmgr_board_name:\s*([^\s\n]+)', file_content)
                if match:
                    bmgr_board_name = match.group(1)
            except OSError:
                pass

        boards.append((board_name, bmgr_board_name, is_from_bmgr))

    return boards


def action_extensions(base_actions: dict, project_path: str) -> dict:
    """Register custom idf.py actions for board selection."""

    def esp_select_board_callback(target_name: str, ctx: Any, args: Any, **kwargs: Any) -> None:
        """Select a board by name and generate board manager configuration."""
        # Check if list flag is set
        if kwargs.get("list", False):
            project_dir = Path(project_path)

            boards = _list_available_boards(BOARDS_DIR)

            if not boards:
                click.echo("No boards found.")
                return

            click.echo("Available boards:")
            click.echo("")

            for board_name, bmgr_board_name, is_from_bmgr in boards:
                board_type = "ESP Board Manager" if is_from_bmgr else "Custom"
                if is_from_bmgr and bmgr_board_name != board_name:
                    click.echo(f"  {board_name} ({board_type}, bmgr: {bmgr_board_name})")
                else:
                    click.echo(f"  {board_name} ({board_type})")

        board_name = kwargs.get("board")
        if not board_name:
            raise click.UsageError("Board name is required. Use: idf.py select-board --board <board_name> or idf.py select-board --list")

        # Set up paths
        # Construct board path using the boards directory containing this file
        board_path = BOARDS_DIR / board_name

        # Validate and normalize board path
        board_path = _validate_board_path(board_path)

        project_dir = Path(project_path)
        gen_bmgr_codes_dir = project_dir / "components" / "gen_bmgr_codes"
        agent_board_name_file = gen_bmgr_codes_dir / BOARD_NAME_FILE
        bmgr_script_path = (
            project_dir / "managed_components" / BMGR_COMPONENT / BMGR_SCRIPT
        )

        # Ensure gen_bmgr_codes directory exists
        gen_bmgr_codes_dir.mkdir(parents=True, exist_ok=True)

        # Write board path file
        _write_board_name_file(board_name, agent_board_name_file)

        # Ensure board manager script exists
        _ensure_bmgr_script_exists(bmgr_script_path, project_dir)

        # Check if board is from ESP Board Manager or custom
        use_from_bmgr_file = board_path / USE_FROM_BMGR_FILE

        if use_from_bmgr_file.exists():
            # Board from ESP Board Manager
            # Parse the file for bmgr_board_name pattern (first occurrence only)
            # Regex searches for: "bmgr_board_name:" followed by optional whitespace,
            # then captures the board name (any non-whitespace characters) until
            # the first whitespace or newline character
            # All other file contents are ignored.
            file_content = use_from_bmgr_file.read_text(encoding="utf-8")
            match = re.search(r'bmgr_board_name:\s*([^\s\n]+)', file_content)
            actual_board_name = match.group(1) if match else board_name
            _run_bmgr_script(bmgr_script_path, project_dir, actual_board_name)
        else:
            # Custom board - validate required files
            _validate_custom_board_files(board_path)
            _run_bmgr_script(bmgr_script_path, project_dir, board_name, board_path)

        # Save the board path file again because the folder contents are deleted by bmgr script.
        _write_board_name_file(board_name, agent_board_name_file)

        click.secho(f"Successfully selected board: {board_name}", fg="green")


    # Get available boards for autocomplete
    available_boards = [board_name for board_name, _, _ in _list_available_boards(BOARDS_DIR)]

    # Define command options for select-board
    select_board_options = [
        {
            "names": ["--board", "-b"],
            "help": "Name of the board (e.g., echoear_core_board_v1_2)",
            "type": BoardChoice(available_boards, case_sensitive=False),
            "required": False,
        },
        {
            "names": ["--list", "-l"],
            "help": "List all available boards",
            "is_flag": True,
        },
    ]

    # Define the actions
    esp_actions = {
        "actions": {
            "select-board": {
                "callback": esp_select_board_callback,
                "options": select_board_options,
                "short_help": "Select a board by name and generate board manager configuration.\nThis will also automatically update the sdkconfig values with boars/sdkconfig.defaults",
            },
        }
    }

    return esp_actions
