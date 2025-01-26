from typing import Callable, Dict, List, Optional, Tuple

def ProcessMessage(
    hist: List[Tuple[str, str]],
    q: str,
    scope: Optional[Dict[str, str]],
) -> str: ...
def ProcessPartial(
    hist: List[Tuple[str, str]],
    q: str,
    scope: Optional[Dict[str, str]],
    call: Callable[[str], None],
) -> str: ...
