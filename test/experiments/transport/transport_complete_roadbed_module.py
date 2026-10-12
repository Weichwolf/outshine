"""Complete public and physical island geometry before all original coupled checks."""

import inspect

import transport_coupled_roadbed_experiment as coupled
from transport_roadbed_island_cleanup import close_split_pockets


def compose_complete_roadbeds(surface,shape,supports,reserved):
    base,core,planned,physical,quantum=coupled.compose_roadbeds(surface,shape,supports)
    complete,audit=close_split_pockets(core,physical,reserved)
    planned=planned.union(complete.difference(physical))
    return (base,core,planned,complete,quantum),audit


def construct_complete_module(network,trial,owner,sections,affected,region,buildings,section_index,required,cache,reserved):
    last={}
    def compose(surface,shape,supports):
        product,audit=compose_complete_roadbeds(surface,shape,supports,reserved)
        last.clear();last.update(audit)
        return product
    namespace=dict(coupled.__dict__,compose_roadbeds=compose)
    code='\n'.join(inspect.getsource(f) for f in (coupled.coupled_addition_rejection,coupled.construct_module))
    exec(compile(code,__file__,'exec'),namespace)
    product=namespace['construct_module'](network,trial,owner,sections,affected,region,buildings,
                                         section_index,required,cache)
    product['physical_island_cleanup']=dict(last)
    return product
