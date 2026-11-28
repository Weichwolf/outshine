"""Allocate one split pattern, including weighted floating sizes and global repeat indices."""

from dataclasses import dataclass
import math

from .scope import Relative,scalar


@dataclass(frozen=True)
class Floating:
    value: float

    def __post_init__(self):
        value=scalar(self.value)
        object.__setattr__(self,'value',value)


@dataclass(frozen=True)
class Part:
    size: object
    label: str=''

    def __post_init__(self):
        value=self.size.value if isinstance(self.size,(Relative,Floating)) else scalar(self.size)
        if value<0:raise ValueError('Split sizes cannot be negative')
        if not isinstance(self.label,str):raise ValueError('Split labels must be strings')


@dataclass(frozen=True)
class Span:
    start: float
    stop: float
    label: str
    pattern_index: int
    index: int
    total: int


def allocations(extent,parts,repeat=False,max_parts=10000):
    extent=scalar(extent);parts=tuple(parts)
    if extent<0:raise ValueError('Split extent cannot be negative')
    if not isinstance(repeat,bool):raise ValueError('Repeat must be boolean')
    if isinstance(max_parts,bool) or not isinstance(max_parts,int) or max_parts<1:raise ValueError('Invalid split expansion limit')
    if any(not isinstance(part,Part) for part in parts):raise ValueError('Split patterns require Part objects')
    if not parts or extent==0:return ()
    sizes=[part.size.value*extent if isinstance(part.size,Relative) else
           part.size.value if isinstance(part.size,Floating) else scalar(part.size) for part in parts]
    if not all(math.isfinite(size) for size in sizes):raise ValueError('Split dimensions exceed finite values')
    fixed=sum(size for size,part in zip(sizes,parts) if not isinstance(part.size,Floating))
    floating=sum(size for size,part in zip(sizes,parts) if isinstance(part.size,Floating))
    preferred=fixed+floating
    if not math.isfinite(preferred):raise ValueError('Split pattern sum exceeds finite values')
    if preferred==0:raise ValueError('Repeated or allocated split must make progress')
    if repeat and (not math.isfinite(extent/preferred) or extent/preferred>max_parts+1):
        raise ValueError('Split exceeds its declared expansion limit')
    if repeat and floating:
        count=max(1,math.floor(extent/preferred))
        candidates=[n for n in (count,count+1) if extent-n*fixed>0]
        if candidates:
            count=min(candidates,key=lambda n:(abs((extent-n*fixed)/(n*floating)-1),n))
            factor=(extent-count*fixed)/(count*floating)
        else:count=1;factor=0.
    elif repeat:
        count=math.ceil(extent/preferred);factor=0.
    else:count=1;factor=max(0.,extent-fixed)/floating if floating else 0.
    positive=sum(size>0 for size in sizes)
    if count*positive>max_parts:raise ValueError('Split exceeds its declared expansion limit')
    widths=[size*factor if isinstance(part.size,Floating) else size for size,part in zip(sizes,parts)]
    rows=[];offset=0.
    for _ in range(count):
        for number,(part,width) in enumerate(zip(parts,widths)):
            if width<=0 or offset>=extent:continue
            stop=min(extent,offset+width)
            if stop<=offset:raise ValueError('Split width is below coordinate precision')
            rows.append((offset,stop,part.label,number));offset=stop
    return tuple(Span(*row,index,len(rows)) for index,row in enumerate(rows))
