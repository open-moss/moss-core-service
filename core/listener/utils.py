from os import path
import json
import yaml

def load_config(config_path):
    if not path.exists(config_path):
        raise FileExistsError(f"listener config is not found: {config_path}")
    with open(config_path, "r", encoding="utf-8") as f:
        data = f.read()
    temp = yaml.load(data, yaml.Loader)
    config = ListenerConfig(**temp)
    return config

class Base():
    def __init__(self, **kwargs):
        for k, v in kwargs.items():
            if type(v) == dict:
                v = Base(**v)
            self[k] = v

    def keys(self):
        return self.__dict__.keys()

    def items(self):
        return self.__dict__.items()

    def values(self):
        return self.__dict__.values()

    def __len__(self):
        return len(self.__dict__)

    def __getitem__(self, key):
        return getattr(self, key)

    def __setitem__(self, key, value):
        return setattr(self, key, value)

    def __contains__(self, key):
        return key in self.__dict__

    def __repr__(self):
        return self.__dict__.__repr__()

class ListenerConfig(Base):
    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        if not hasattr(self, "sample_rate"):
            self.sample_rate = 16000
        if not hasattr(self, "model_dir_path"):
            raise ValueError("model_dir_path is required")
    
